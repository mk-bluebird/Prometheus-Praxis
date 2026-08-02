// File: cpp/tools/eco_config_loader.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace eco {

class EcoConfigLoader {
public:
    std::unordered_map<std::string, std::string> load(const std::string &path) const {
        std::unordered_map<std::string, std::string> cfg;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Cannot open config: " << path << "\n";
            return cfg;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            cfg[key] = value;
        }
        return cfg;
    }
};

} // namespace eco

int main() {
    eco::EcoConfigLoader loader;
    auto cfg = loader.load("eco.conf");
    for (const auto &kv : cfg) {
        std::cout << kv.first << " = " << kv.second << "\n";
    }
    return 0;
}
