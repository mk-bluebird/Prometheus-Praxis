// filename: cpp/core/ppx_cli.hpp
#pragma once
#include <string>
#include <vector>

namespace ppx {

struct CliOption {
    std::string name;       // "--input"
    std::string value;      // "path/to/file.csv"
};

struct CliArgs {
    std::vector<CliOption> options;
};

inline CliArgs parse_cli(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string token(argv[i]);
        if (token.rfind("--", 0) == 0 && i + 1 < argc) {
            args.options.push_back(CliOption{token, std::string(argv[i + 1])});
            ++i;
        }
    }
    return args;
}

inline std::string get_option(const CliArgs& args, const std::string& name, const std::string& def = "") {
    for (const auto& opt : args.options) {
        if (opt.name == name) return opt.value;
    }
    return def;
}

} // namespace ppx
