// File: cpp/include/eco_errors.hpp
#ifndef ECO_ERRORS_HPP
#define ECO_ERRORS_HPP

#include <string>

/**
 * @file eco_errors.hpp
 * @brief Error taxonomy for eco-restoration C++ tools.
 *
 * Distinguishes corridor breaches, invalid configuration, database errors,
 * and ALN conformance mismatches, so automated tooling and AI-chat can
 * understand failure modes and respond appropriately.[59][78]
 */

namespace eco_errors {

/**
 * @brief Enumerated error codes for eco tools.
 */
enum class EcoErrorCode {
    NONE = 0,
    CORRIDOR_BREACH,      ///< Corridor rule violated (e.g., deltaVt>1, PFAS mass increase under cold-survival).
    INVALID_CONFIG,       ///< Configuration file or parameters invalid or out of corridor range.
    DB_ERROR,             ///< SQLite or database operation failed.
    ALN_CONFORMANCE_FAIL, ///< C++ parameters exceed ALN-declared corridors or KER ranges.
    SERIALIZATION_ERROR,  ///< Failure to read/write JSON/CSV eco artifacts.
    UNKNOWN               ///< Unclassified error.
};

/**
 * @brief Lightweight error object carrying code and message.
 */
struct EcoError {
    EcoErrorCode code;
    std::string message;
};

/**
 * @brief Convert an error code to a human-readable string.
 */
inline std::string to_string(EcoErrorCode code) {
    switch (code) {
        case EcoErrorCode::NONE:               return "NONE";
        case EcoErrorCode::CORRIDOR_BREACH:    return "CORRIDOR_BREACH";
        case EcoErrorCode::INVALID_CONFIG:     return "INVALID_CONFIG";
        case EcoErrorCode::DB_ERROR:           return "DB_ERROR";
        case EcoErrorCode::ALN_CONFORMANCE_FAIL:return "ALN_CONFORMANCE_FAIL";
        case EcoErrorCode::SERIALIZATION_ERROR:return "SERIALIZATION_ERROR";
        case EcoErrorCode::UNKNOWN:            return "UNKNOWN";
        default:                               return "UNKNOWN";
    }
}

/**
 * @brief Construct an EcoError with code and message.
 */
inline EcoError make_error(EcoErrorCode code, const std::string& msg) {
    EcoError e{};
    e.code = code;
    e.message = msg;
    return e;
}

} // namespace eco_errors

#endif // ECO_ERRORS_HPP
