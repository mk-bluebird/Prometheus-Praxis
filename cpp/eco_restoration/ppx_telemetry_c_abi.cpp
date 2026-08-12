// File: cpp/eco_restoration/ppx_telemetry_c_abi.cpp
#include "ppx_telemetry_c_abi.h"

#include <cmath>
#include <cstring>

namespace {

bool non_empty_text(const char* text, std::size_t capacity) {
    return text != nullptr && std::memchr(text, '\0', capacity) != nullptr && text[0] != '\0';
}

bool unit_coordinate(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

}  // namespace

extern "C" int ppx_telemetry_c_valid(const TelemetryC* telemetry) {
    if (telemetry == nullptr ||
        telemetry->abi_version != PPX_TELEMETRY_ABI_VERSION ||
        telemetry->byte_size != sizeof(TelemetryC) ||
        !non_empty_text(telemetry->machine_id, sizeof(telemetry->machine_id)) ||
        !non_empty_text(telemetry->station_id, sizeof(telemetry->station_id)) ||
        !non_empty_text(telemetry->timestamp_utc, sizeof(telemetry->timestamp_utc)) ||
        !std::isfinite(telemetry->vt_current) || !std::isfinite(telemetry->vt_next)) {
        return 0;
    }

    return unit_coordinate(telemetry->r_hydraulics) &&
           unit_coordinate(telemetry->r_energy) &&
           unit_coordinate(telemetry->r_uncertainty) &&
           unit_coordinate(telemetry->r_reliability) &&
           unit_coordinate(telemetry->r_extra_1) &&
           unit_coordinate(telemetry->r_extra_2) &&
           unit_coordinate(telemetry->roh);
}
