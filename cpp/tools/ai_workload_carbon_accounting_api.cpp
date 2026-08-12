// File: cpp/tools/ai_workload_carbon_accounting_api.cpp
#include <httplib.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

struct GarchModel {
    double mean{}, omega{}, alpha{}, beta{}, last_residual{}, last_variance{};
};

GarchModel load_model(sqlite3* database) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(database,
        "SELECT mean,omega,alpha,beta,last_residual,last_variance "
        "FROM carbon_garch_model ORDER BY rowid DESC LIMIT 1;",
        -1, &raw, nullptr) != SQLITE_OK)
        throw std::runtime_error("carbon GARCH model unavailable");
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) throw std::runtime_error("carbon GARCH model absent");
    return {sqlite3_column_double(statement.get(), 0), sqlite3_column_double(statement.get(), 1),
            sqlite3_column_double(statement.get(), 2), sqlite3_column_double(statement.get(), 3),
            sqlite3_column_double(statement.get(), 4), sqlite3_column_double(statement.get(), 5)};
}

double value(const httplib::Request& request, const char* key) {
    if (!request.has_param(key)) throw std::invalid_argument("missing parameter");
    const std::string text = request.get_param_value(key);
    std::size_t consumed = 0;
    const double result = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(result)) throw std::invalid_argument("invalid parameter");
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(argv[1], &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);

    httplib::Server server;
    server.Post("/v1/carbon-accounting", [&database](const httplib::Request& request,
                                                      httplib::Response& response) {
        try {
            const double duration_s = value(request, "duration_s");
            const double power_w = value(request, "power_w");
            const double renewable_fraction = value(request, "renewable_fraction");
            const double embodied_g_per_kwh = value(request, "embodied_g_per_kwh");
            if (duration_s < 0.0 || power_w < 0.0 || renewable_fraction < 0.0 ||
                renewable_fraction > 1.0 || embodied_g_per_kwh < 0.0)
                throw std::invalid_argument("parameter outside physical bounds");

            const GarchModel model = load_model(database.get());
            const double variance = std::max(1e-9, model.omega +
                model.alpha * model.last_residual * model.last_residual + model.beta * model.last_variance);
            const double forecast = std::max(0.0, model.mean);
            const double energy_kwh = duration_s * power_w / 3600000.0;
            const double operational_g = energy_kwh * forecast * (1.0 - renewable_fraction);
            const double embodied_g = energy_kwh * embodied_g_per_kwh;
            response.set_content("{\"energy_kwh\":" + std::to_string(energy_kwh) +
                ",\"forecast_carbon_g_kwh\":" + std::to_string(forecast) +
                ",\"forecast_sd_g_kwh\":" + std::to_string(std::sqrt(variance)) +
                ",\"operational_carbon_g\":" + std::to_string(operational_g) +
                ",\"embodied_carbon_g\":" + std::to_string(embodied_g) +
                ",\"total_carbon_g\":" + std::to_string(operational_g + embodied_g) + "}",
                "application/json");
        } catch (const std::exception& error) {
            response.status = 400;
            response.set_content("{\"error\":\"" + std::string(error.what()) + "\"}", "application/json");
        }
    });
    return server.listen("127.0.0.1", std::stoi(argv[2])) ? 0 : 1;
}
