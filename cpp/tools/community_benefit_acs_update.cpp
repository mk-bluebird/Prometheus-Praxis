// File: cpp/tools/community_benefit_acs_update.cpp
#include <curl/curl.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

size_t append_response(char* data, size_t size, size_t count, void* target) {
    static_cast<std::string*>(target)->append(data, size * count);
    return size * count;
}

std::vector<std::string> first_acs_record(const std::string& json) {
    const auto first = json.find('[', json.find('[') + 1);
    const auto last = json.find(']', first);
    if (first == std::string::npos || last == std::string::npos) throw std::runtime_error("invalid ACS response");
    std::vector<std::string> fields;
    std::string value;
    bool quoted = false;
    for (std::size_t i = first + 1; i < last; ++i) {
        if (json[i] == '"') quoted = !quoted;
        else if (json[i] == ',' && !quoted) { fields.push_back(value); value.clear(); }
        else if (json[i] != '[' && json[i] != ']' && json[i] != '"') value += json[i];
    }
    if (!value.empty()) fields.push_back(value);
    return fields;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 7) return 2;
    const double population_reference = std::stod(argv[3]);
    const double income_reference = std::stod(argv[4]);
    const double poverty_reference = std::stod(argv[5]);
    const auto observed_unix_s = static_cast<sqlite3_int64>(std::stoll(argv[6]));
    if (population_reference <= 0.0 || income_reference <= 0.0 || poverty_reference <= 0.0) return 2;

    std::string response;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> request(curl_easy_init(), curl_easy_cleanup);
    if (!request) return 1;
    curl_easy_setopt(request.get(), CURLOPT_URL, argv[2]);
    curl_easy_setopt(request.get(), CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(request.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(request.get(), CURLOPT_TIMEOUT, 20L);
    if (curl_easy_perform(request.get()) != CURLE_OK) return 1;
    const auto fields = first_acs_record(response);
    if (fields.size() < 3) return 1;

    const double population = std::max(0.0, std::stod(fields[0]));
    const double median_income = std::max(0.0, std::stod(fields[1]));
    const double poverty_count = std::max(0.0, std::stod(fields[2]));
    const double poverty_fraction = std::clamp(poverty_count / std::max(1.0, population), 0.0, 1.0);
    const double benefit = std::clamp(
        0.35 * std::min(1.0, population / population_reference) +
        0.40 * std::min(1.0, median_income / income_reference) +
        0.25 * (1.0 - std::min(1.0, poverty_fraction / poverty_reference)), 0.0, 1.0);

    sqlite3* raw = nullptr;
    if (sqlite3_open(argv[1], &raw) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
    sqlite3_exec(database.get(),
        "CREATE TABLE IF NOT EXISTS community_benefit_index("
        "observed_unix_s INTEGER PRIMARY KEY,population REAL NOT NULL,median_income REAL NOT NULL,"
        "poverty_fraction REAL NOT NULL CHECK(poverty_fraction BETWEEN 0 AND 1),"
        "benefit_index REAL NOT NULL CHECK(benefit_index BETWEEN 0 AND 1),source_url TEXT NOT NULL) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* raw_statement = nullptr;
    sqlite3_prepare_v2(database.get(),
        "INSERT INTO community_benefit_index VALUES(?,?,?,?,?,?) "
        "ON CONFLICT(observed_unix_s) DO UPDATE SET population=excluded.population,"
        "median_income=excluded.median_income,poverty_fraction=excluded.poverty_fraction,"
        "benefit_index=excluded.benefit_index,source_url=excluded.source_url;",
        -1, &raw_statement, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw_statement, sqlite3_finalize);
    sqlite3_bind_int64(statement.get(), 1, observed_unix_s);
    sqlite3_bind_double(statement.get(), 2, population);
    sqlite3_bind_double(statement.get(), 3, median_income);
    sqlite3_bind_double(statement.get(), 4, poverty_fraction);
    sqlite3_bind_double(statement.get(), 5, benefit);
    sqlite3_bind_text(statement.get(), 6, argv[2], -1, SQLITE_TRANSIENT);
    return sqlite3_step(statement.get()) == SQLITE_DONE ? 0 : 1;
}
