// File: cpp/eco_restoration/graph_optimal_transport_hex_allocation.cpp

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <queue>
#include <stdexcept>
#include <sqlite3.h>

// This file implements a graph-constrained optimal transport / min-cost flow
// solver for hex resource allocation, using a successive shortest path algorithm
// (network simplex-style) in pure C++. It:
//   - Reads supply/demand and edge costs from SQL tables.
//   - Solves for optimal flows minimizing total cost (including marginal LST reduction).
//   - Writes corridor flows into a corridor_plan table for budget/RESTORATION planning.
//
// SQL schema assumptions:
//
//   CREATE TABLE hex_supply (
//       h3_index TEXT PRIMARY KEY,
//       supply   REAL NOT NULL      -- positive for source hexes, 0 otherwise
//   );
//
//   CREATE TABLE hex_demand (
//       h3_index TEXT PRIMARY KEY,
//       demand   REAL NOT NULL      -- positive for sink hexes, 0 otherwise
//   );
//
//   CREATE TABLE hex_edges (
//       edge_id              INTEGER PRIMARY KEY AUTOINCREMENT,
//       from_h3              TEXT NOT NULL,
//       to_h3                TEXT NOT NULL,
//       base_cost            REAL NOT NULL,  -- baseline transport cost
//       lst_reduction_coef   REAL NOT NULL   -- marginal LST reduction benefit (causal forest)
//   );
//
//   CREATE TABLE corridor_plan (
//       edge_id INTEGER PRIMARY KEY,
//       from_h3 TEXT NOT NULL,
//       to_h3   TEXT NOT NULL,
//       flow    REAL NOT NULL,
//       total_cost REAL NOT NULL
//   );

namespace prometheus_praxis {
namespace eco_restoration {

struct Node {
    std::string h3_index;
    double b; // net balance: supply - demand
};

struct Edge {
    int edge_id;
    int from;
    int to;
    double cost;
    double capacity;
};

struct FlowResult {
    int edge_id;
    std::string from_h3;
    std::string to_h3;
    double flow;
    double total_cost;
};

class SqlTransportData {
public:
    explicit SqlTransportData(const std::string& db_path)
        : db_path_(db_path) {}

    void load(std::vector<Node>& nodes,
              std::vector<Edge>& edges,
              double beta_lst_tradeoff) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB: " + msg);
        }

        // Load supply and demand into node map.
        std::unordered_map<std::string, Node> node_map;

        {
            const char* sql = "SELECT h3_index, supply FROM hex_supply;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_close(db);
                throw std::runtime_error("Prepare hex_supply failed: " + msg);
            }
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const unsigned char* h3 = sqlite3_column_text(stmt, 0);
                double supply = sqlite3_column_double(stmt, 1);
                std::string key = h3 ? reinterpret_cast<const char*>(h3) : "";
                Node n{};
                n.h3_index = key;
                n.b = supply; // start with supply
                node_map[key] = n;
            }
            sqlite3_finalize(stmt);
        }

        {
            const char* sql = "SELECT h3_index, demand FROM hex_demand;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_close(db);
                throw std::runtime_error("Prepare hex_demand failed: " + msg);
            }
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const unsigned char* h3 = sqlite3_column_text(stmt, 0);
                double demand = sqlite3_column_double(stmt, 1);
                std::string key = h3 ? reinterpret_cast<const char*>(h3) : "";
                auto it = node_map.find(key);
                if (it == node_map.end()) {
                    Node n{};
                    n.h3_index = key;
                    n.b = -demand;
                    node_map[key] = n;
                } else {
                    it->second.b -= demand;
                }
            }
            sqlite3_finalize(stmt);
        }

        // Convert node_map to vector and index mapping.
        std::unordered_map<std::string, int> node_index;
        nodes.clear();
        nodes.reserve(node_map.size());
        int idx = 0;
        for (const auto& kv : node_map) {
            node_index[kv.first] = idx;
            nodes.push_back(kv.second);
            idx++;
        }

        // Load edges, applying causal forest LST reduction to cost:
        // effective_cost = base_cost - beta * lst_reduction_coef.
        edges.clear();
        {
            const char* sql =
                "SELECT edge_id, from_h3, to_h3, base_cost, lst_reduction_coef "
                "FROM hex_edges;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_close(db);
                throw std::runtime_error("Prepare hex_edges failed: " + msg);
            }
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                int edge_id = sqlite3_column_int(stmt, 0);
                const unsigned char* from_h3 = sqlite3_column_text(stmt, 1);
                const unsigned char* to_h3   = sqlite3_column_text(stmt, 2);
                double base_cost = sqlite3_column_double(stmt, 3);
                double lst_coef  = sqlite3_column_double(stmt, 4);

                std::string from_key = from_h3 ? reinterpret_cast<const char*>(from_h3) : "";
                std::string to_key   = to_h3   ? reinterpret_cast<const char*>(to_h3)   : "";

                auto it_from = node_index.find(from_key);
                auto it_to   = node_index.find(to_key);
                if (it_from == node_index.end() || it_to == node_index.end()) {
                    continue; // skip edges with unknown nodes
                }

                Edge e{};
                e.edge_id = edge_id;
                e.from = it_from->second;
                e.to   = it_to->second;
                e.cost = base_cost - beta_lst_tradeoff * lst_coef; // carbon-aware cost
                e.capacity = std::numeric_limits<double>::infinity(); // can be tuned
                edges.push_back(e);
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);

        // Check total supply/demand balance.
        double total_balance = 0.0;
        for (const auto& n : nodes) {
            total_balance += n.b;
        }
        if (std::fabs(total_balance) > 1e-6) {
            throw std::runtime_error("Supply and demand are not balanced; total_balance=" +
                                     std::to_string(total_balance));
        }
    }

    void writeCorridorPlan(const std::vector<FlowResult>& flows) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for corridor_plan write: " + msg);
        }

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("BEGIN TRANSACTION failed: " + msg);
        }

        const char* sql_del = "DELETE FROM corridor_plan;";
        rc = sqlite3_exec(db, sql_del, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw std::runtime_error("Failed to clear corridor_plan: " + msg);
        }

        const char* sql_ins =
            "INSERT INTO corridor_plan(edge_id, from_h3, to_h3, flow, total_cost) "
            "VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, sql_ins, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw std::runtime_error("Prepare insert failed: " + msg);
        }

        for (const auto& f : flows) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            rc = sqlite3_bind_int(stmt, 1, f.edge_id);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_text(stmt, 2, f.from_h3.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_text(stmt, 3, f.to_h3.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 4, f.flow);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 5, f.total_cost);
            if (rc != SQLITE_OK) goto bind_error;

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Insert step failed: " + msg);
            }
            continue;

        bind_error:
            {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Bind error: " + msg);
            }
        }

        sqlite3_finalize(stmt);
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("COMMIT failed: " + msg);
        }

        sqlite3_close(db);
    }

private:
    std::string db_path_;
};

// Min-cost flow solver using successive shortest paths with potentials.
// This is conceptually equivalent to network simplex on a balanced network.
class MinCostFlowSolver {
public:
    MinCostFlowSolver(const std::vector<Node>& nodes,
                      const std::vector<Edge>& edges)
        : nodes_(nodes),
          edges_(edges) {
        n_ = nodes_.size();
        m_ = edges_.size();
        buildAdjacency();
    }

    std::vector<double> solve() {
        // Initialize potentials and flows.
        std::vector<double> pi(n_, 0.0);          // node potentials
        std::vector<double> flow(m_, 0.0);        // edge flows
        std::vector<double> residual_cap(m_, 0.0);

        for (std::size_t e = 0; e < m_; ++e) {
            residual_cap[e] = edges_[e].capacity;
        }

        // Construct supply vector.
        std::vector<double> b(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            b[i] = nodes_[i].b;
        }

        // Successive shortest path: repeatedly send flow from supply nodes to demand nodes.
        while (true) {
            int s = -1;
            int t = -1;
            for (std::size_t i = 0; i < n_; ++i) {
                if (b[i] > 1e-9) s = static_cast<int>(i);
                else if (b[i] < -1e-9) t = static_cast<int>(i);
                if (s != -1 && t != -1) break;
            }
            if (s == -1 || t == -1) break; // no more imbalances

            // Run shortest path from s to t on residual graph w.r.t. reduced costs.
            std::vector<double> dist(n_, std::numeric_limits<double>::infinity());
            std::vector<int> prev_edge(n_, -1);
            dist[s] = 0.0;

            using State = std::pair<double, int>;
            std::priority_queue<State, std::vector<State>, std::greater<State>> pq;
            pq.push({0.0, s});

            while (!pq.empty()) {
                auto [d, u] = pq.top();
                pq.pop();
                if (d > dist[u] + 1e-12) continue;
                if (u == t) break;

                for (int ei : adj_[u]) {
                    int v = edges_[ei].to;
                    if (residual_cap[ei] <= 1e-9) continue;
                    double rcost = edges_[ei].cost + pi[u] - pi[v];
                    double nd = d + rcost;
                    if (nd < dist[v] - 1e-12) {
                        dist[v] = nd;
                        prev_edge[v] = ei;
                        pq.push({nd, v});
                    }
                }
            }

            if (prev_edge[t] == -1) {
                throw std::runtime_error("No augmenting path found; network may be disconnected");
            }

            // Determine augmenting amount.
            double send = b[s];
            send = std::min(send, -b[t]);
            int v = t;
            while (v != s) {
                int ei = prev_edge[v];
                send = std::min(send, residual_cap[ei]);
                v = edges_[ei].from;
            }

            // Augment along path.
            v = t;
            while (v != s) {
                int ei = prev_edge[v];
                flow[ei] += send;
                residual_cap[ei] -= send;
                v = edges_[ei].from;
            }

            // Update supply/demand.
            b[s] -= send;
            b[t] += send;

            // Update potentials.
            for (std::size_t i = 0; i < n_; ++i) {
                if (dist[i] < std::numeric_limits<double>::infinity()) {
                    pi[i] += dist[i] - dist[t];
                }
            }
        }

        return flow;
    }

private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    std::size_t n_;
    std::size_t m_;
    std::vector<std::vector<int>> adj_;

    void buildAdjacency() {
        adj_.assign(n_, {});
        for (std::size_t e = 0; e < m_; ++e) {
            adj_[edges_[e].from].push_back(static_cast<int>(e));
        }
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "hex_transport.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    double beta_lst_tradeoff = 1.0; // trade-off weight: higher favors LST reduction in costs

    try {
        SqlTransportData sql_data(db_path);

        std::vector<Node> nodes;
        std::vector<Edge> edges;
        sql_data.load(nodes, edges, beta_lst_tradeoff);

        MinCostFlowSolver solver(nodes, edges);
        std::vector<double> flows = solver.solve();

        // Build FlowResult for corridor_plan output.
        std::vector<FlowResult> results;
        results.reserve(edges.size());
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if (flows[e] <= 1e-9) continue;
            FlowResult fr{};
            fr.edge_id = edges[e].edge_id;
            fr.from_h3 = nodes[edges[e].from].h3_index;
            fr.to_h3   = nodes[edges[e].to].h3_index;
            fr.flow    = flows[e];
            fr.total_cost = flows[e] * edges[e].cost;
            results.push_back(fr);
        }

        sql_data.writeCorridorPlan(results);

        std::cout << "Graph-constrained optimal transport solved; corridor_plan table written." << std::endl;
        std::cout << "Use corridor_plan together with equity and LST metrics for visualization and governance." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Optimal transport error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
