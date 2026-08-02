// File: cpp/tools/eco_logger.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>

namespace eco {

class EcoLogger {
public:
    explicit EcoLogger(const std::string &path) : file_(path, std::ios::app) {
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << path << "\n";
        }
    }

    void log(const std::string &message) {
        if (!file_.is_open()) return;
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        file_ << std::ctime(&t) << ": " << message << "\n";
    }

private:
    std::ofstream file_;
};

} // namespace eco

int main() {
    eco::EcoLogger logger("eco.log");
    logger.log("Eco system initialized.");
    logger.log("Sample eco-impact computation completed.");
    return 0;
}
