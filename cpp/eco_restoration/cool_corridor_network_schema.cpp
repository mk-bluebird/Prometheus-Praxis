// File: cpp/eco_restoration/cool_corridor_network_schema.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Install SQL schema for cool corridor network: nodes, edges, and network plans.
void install_cool_corridor_schema(sqlite3* db) {
    // Nodes: each node is typically an H3 hex centroid or important junction.
    const std::string sql_nodes = R"SQL(
        CREATE TABLE IF NOT EXISTS cool_corridor_node (
            node_id        INTEGER PRIMARY KEY AUTOINCREMENT,
            h3_index       TEXT UNIQUE,
            lat            REAL NOT NULL,
            lon            REAL NOT NULL,
            lst_c          REAL,           -- local land surface temperature (C)
            vulnerability  REAL,           -- heat-vulnerability score 0..1
            equity_weight  REAL,           -- equity weight for corridor planning
            updated_at     TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_cool_corridor_node_h3
            ON cool_corridor_node(h3_index);
    )SQL";

    // Edges: neighborhood graph with edge weights combining distance and LST gradient.
    // Edge weight formula conceptually:
    //   w_edge = alpha_dist * distance_km + alpha_lst * max(0, lst_upstream - lst_downstream)
    // where alpha_dist, alpha_lst are planning weights.
    const std::string sql_edges = R"SQL(
        CREATE TABLE IF NOT EXISTS cool_corridor_edge (
            edge_id        INTEGER PRIMARY KEY AUTOINCREMENT,
            from_node_id   INTEGER NOT NULL,
            to_node_id     INTEGER NOT NULL,
            distance_km    REAL NOT NULL,
            lst_grad_c     REAL,           -- LST difference (from - to)
            edge_weight    REAL NOT NULL,  -- combined distance + LST gradient cost
            cooling_benefit REAL,          -- estimated cumulative cooling benefit if built
            updated_at     TEXT NOT NULL,
            FOREIGN KEY (from_node_id) REFERENCES cool_corridor_node(node_id) ON DELETE CASCADE,
            FOREIGN KEY (to_node_id)   REFERENCES cool_corridor_node(node_id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_cool_corridor_edge_from_to
            ON cool_corridor_edge(from_node_id, to_node_id);
    )SQL";

    // Network plans: store corridors (paths) between start/end nodes with cumulative metrics.
    const std::string sql_network = R"SQL(
        CREATE TABLE IF NOT EXISTS cool_corridor_network (
            plan_id            INTEGER PRIMARY KEY AUTOINCREMENT,
            plan_name          TEXT NOT NULL,
            start_node_id      INTEGER NOT NULL,
            end_node_id        INTEGER NOT NULL,
            total_edge_weight  REAL NOT NULL,
            total_distance_km  REAL NOT NULL,
            total_cooling_benefit REAL NOT NULL,
            equity_score       REAL,           -- equity metric for the corridor
            budget_cost        REAL,           -- estimated cost of implementation
            created_at         TEXT NOT NULL,
            FOREIGN KEY (start_node_id) REFERENCES cool_corridor_node(node_id),
            FOREIGN KEY (end_node_id)   REFERENCES cool_corridor_node(node_id)
        );

        CREATE TABLE IF NOT EXISTS cool_corridor_path_segment (
            plan_id       INTEGER NOT NULL,
            seq_index     INTEGER NOT NULL,
            node_id       INTEGER NOT NULL,
            edge_id       INTEGER,         -- edge leading to this node (NULL for start)
            cumulative_distance_km REAL NOT NULL,
            cumulative_cooling_benefit REAL NOT NULL,
            PRIMARY KEY (plan_id, seq_index),
            FOREIGN KEY (plan_id) REFERENCES cool_corridor_network(plan_id) ON DELETE CASCADE,
            FOREIGN KEY (node_id) REFERENCES cool_corridor_node(node_id),
            FOREIGN KEY (edge_id) REFERENCES cool_corridor_edge(edge_id)
        );
    )SQL";

    exec_sql(db, sql_nodes);
    exec_sql(db, sql_edges);
    exec_sql(db, sql_network);
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "cool_corridor.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try {
        install_cool_corridor_schema(db);
        std::cout << "Cool corridor network schema installed into " << db_path << std::endl;

        std::cout << "\n-- Example query: list nodes --\n";
        std::cout << "SELECT node_id, h3_index, lst_c, vulnerability, equity_weight FROM cool_corridor_node LIMIT 10;\n";

        std::cout << "\n-- Example query: list edges with weights --\n";
        std::cout << "SELECT edge_id, from_node_id, to_node_id, distance_km, lst_grad_c, edge_weight FROM cool_corridor_edge LIMIT 10;\n";

        std::cout << "\n-- Example query: retrieve a corridor plan --\n";
        std::cout << "SELECT * FROM cool_corridor_network WHERE plan_id = 1;\n";
        std::cout << "SELECT * FROM cool_corridor_path_segment WHERE plan_id = 1 ORDER BY seq_index;\n";
    } catch (const std::exception& ex) {
        std::cerr << "Schema installation error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
