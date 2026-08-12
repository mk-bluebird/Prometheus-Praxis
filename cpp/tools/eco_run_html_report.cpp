// File: cpp/tools/eco_run_html_report.cpp
#include <sqlite3.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Point { double x{}; double y{}; };

std::string escape_html(const std::string& text) {
    std::string escaped;
    for (char c : text) {
        if (c == '&') escaped += "&amp;";
        else if (c == '<') escaped += "&lt;";
        else if (c == '>') escaped += "&gt;";
        else if (c == '"') escaped += "&quot;";
        else escaped += c;
    }
    return escaped;
}

bool table_exists(sqlite3* database, const char* table) {
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;", -1,
                       &statement, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close(statement, sqlite3_finalize);
    sqlite3_bind_text(statement, 1, table, -1, SQLITE_STATIC);
    return sqlite3_step(statement) == SQLITE_ROW;
}

double scalar(sqlite3* database, const char* query) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, query, -1, &statement, nullptr) != SQLITE_OK) return 0.0;
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close(statement, sqlite3_finalize);
    return sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_double(statement, 0) : 0.0;
}

std::vector<Point> points(sqlite3* database, const char* query) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, query, -1, &statement, nullptr) != SQLITE_OK) return {};
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close(statement, sqlite3_finalize);
    std::vector<Point> result;
    while (sqlite3_step(statement) == SQLITE_ROW)
        result.push_back({sqlite3_column_double(statement, 0), sqlite3_column_double(statement, 1)});
    return result;
}

std::string svg_polyline(const std::vector<Point>& data, const std::string& color) {
    if (data.empty()) return "<p>No source data available.</p>";
    double min_x = data.front().x, max_x = min_x, min_y = data.front().y, max_y = min_y;
    for (const auto& p : data) {
        min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
    }
    const auto map = [](double v, double low, double high, double output_low, double output_high) {
        return high == low ? (output_low + output_high) / 2.0 :
            output_low + (v - low) * (output_high - output_low) / (high - low);
    };
    std::ostringstream output;
    output << "<svg viewBox='0 0 640 260' role='img'><rect width='640' height='260' fill='white' "
           << "stroke='#777'/><polyline fill='none' stroke='" << color << "' stroke-width='3' points='";
    for (const auto& p : data)
        output << map(p.x, min_x, max_x, 30, 610) << ',' << map(p.y, min_y, max_y, 230, 30) << ' ';
    return output.str() + "'/></svg>";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: eco_run_html_report <run.sqlite> <report.html>\n";
        return 2;
    }
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(argv[1], &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);

    const bool has_environment = table_exists(database.get(), "hex_environment_observation");
    const bool has_pareto = table_exists(database.get(), "lane_threshold_pareto");
    const double observations = has_environment ? scalar(database.get(),
        "SELECT COUNT(*) FROM hex_environment_observation;") : 0.0;
    const double average_heat = has_environment ? scalar(database.get(),
        "SELECT AVG(heat_risk) FROM hex_environment_observation;") : 0.0;
    const double average_water = has_environment ? scalar(database.get(),
        "SELECT AVG(water_risk) FROM hex_environment_observation;") : 0.0;

    const auto trend = has_environment ? points(database.get(),
        "SELECT observed_unix_s,AVG(MAX(heat_risk,water_risk,biodiversity_risk)) "
        "FROM hex_environment_observation GROUP BY observed_unix_s ORDER BY observed_unix_s;")
        : std::vector<Point>{};
    const auto pareto = has_pareto ? points(database.get(),
        "SELECT false_negative_halts,budget_excess FROM lane_threshold_pareto "
        "WHERE pareto_rank=0 ORDER BY false_negative_halts;") : std::vector<Point>{};

    std::ofstream output(argv[2]);
    if (!output) return 1;
    output << "<!doctype html><html><head><meta charset='utf-8'><title>Eco-restoration run report</title>"
           << "<style>body{font-family:sans-serif;max-width:900px;margin:auto}section{margin:2em 0}"
           << ".card{display:inline-block;padding:1em;margin:.3em;background:#eef6ee}</style></head><body>"
           << "<h1>Eco-restoration run report</h1><section><h2>Summary</h2>"
           << "<div class='card'>Observations: " << std::llround(observations) << "</div>"
           << "<div class='card'>Mean heat risk: " << std::fixed << std::setprecision(3) << average_heat << "</div>"
           << "<div class='card'>Mean water risk: " << average_water << "</div></section>"
           << "<section><h2>Environmental-risk trend</h2>" << svg_polyline(trend, "#b33") << "</section>"
           << "<section><h2>Pareto front: false-negative halts vs budget excess</h2>"
           << svg_polyline(pareto, "#1769aa") << "</section></body></html>";
}
