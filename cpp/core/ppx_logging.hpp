// filename: cpp/core/ppx_logging.hpp
#pragma once
#include <iostream>
#include <string>
#include "ppx_types.hpp"

namespace ppx {

enum class LogLevel { Debug, Info, Warn, Error };

inline const char* to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

inline void log(LogLevel level, const std::string& msg) {
    std::cerr << "[" << to_string(level) << "] " << msg << "\n";
}

inline void log_ker_snapshot(LogLevel level, const KerSnapshot& ks) {
    std::string msg = "node=" + ks.node_id.region + "/" + ks.node_id.system + "/" + ks.node_id.node +
                      " vt=" + std::to_string(ks.vt) +
                      " K=" + std::to_string(ks.k) +
                      " E=" + std::to_string(ks.e) +
                      " R=" + std::to_string(ks.r);
    log(level, msg);
}

} // namespace ppx
