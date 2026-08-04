// File: cpp/eco_restoration/fair_hex_allocation.cpp

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <sqlite3.h>

struct HexState {
    std::string h3_index;
    double lst_after;    // post-intervention LST (°C)
    double green_units;  // allocated restoration units
};

class FairHexAllocation {
public:
    FairHexAllocation(sqlite3* db, double budget_units)
        : db_(db), budget_(budget_units) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
        if (budget_ < 0.0) {
            throw std::runtime_error("Budget must be non-negative");
        }
    }

    double computeGini(const std::vector<HexState>& hexes) const {
        if (hexes.empty()) {
            return 0.0;
        }
        std::size_t n = hexes.size();
        std::vector<double> lst;
        lst.reserve(n);
        for (const auto& h : hexes) {
            lst.push_back(h.lst_after);
        }
        std::sort(lst.begin(), lst.end());
        double sum = 0.0;
        for (double v : lst) {
            sum += v;
        }
        if (sum <= 0.0) {
            return 0.0;
        }

        double gini = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            gini += (2.0 * static_cast<double>(i + 1) - static_cast<double>(n) - 1.0) * lst[i];
        }
        gini /= (static_cast<double>(n) * sum);
        return gini;
    }

    double equityPenaltyKerE(const std::vector<HexState>& hexes) const {
        double gini = computeGini(hexes);
        double beta = 1.0;
        return beta * gini;
    }

    std::vector<HexState> allocate(
        const std::vector<HexState>& baseline,
        const std::unordered_map<std::string, double>& causalCateLstDropPerUnit
    ) const {
        std::vector<HexState> result = baseline;
        double remaining = budget_;
        if (result.empty() || remaining <= 0.0) {
            return result;
        }

        std::vector<std::size_t> idx(result.size());
        for (std::size_t i = 0; i < result.size(); ++i) {
            idx[i] = i;
        }

        std::sort(
            idx.begin(),
            idx.end(),
            [&](std::size_t a, std::size_t b) {
                double cateA = causalCate(result[a].h3_index, causalCateLstDropPerUnit);
                double cateB = causalCate(result[b].h3_index, causalCateLstDropPerUnit);
                double scoreA = cateA * result[a].lst_after;
                double scoreB = cateB * result[b].lst_after;
                return scoreA > scoreB;
            }
        );

        for (std::size_t k = 0; k < idx.size() && remaining > 0.0; ++k) {
            HexState& h = result[idx[k]];
            double cate = causalCate(h.h3_index, causalCateLstDropPerUnit);
            if (cate <= 0.0) {
                continue;
            }
            h.green_units += 1.0;
            h.lst_after -= cate;
            remaining -= 1.0;
        }

        return result;
    }

private:
    sqlite3* db_;
    double   budget_;

    double causalCate(
        const std::string& h3_index,
        const std::unordered_map<std::string, double>& m
    ) const {
        auto it = m.find(h3_index);
        if (it == m.end()) {
            return 0.0;
        }
        return it->second;
    }
};
