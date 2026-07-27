#include "ppx/waste/shredding_governance_adapter.hpp"

#include <stdexcept>
#include <cstring>
#include <nlohmann/json.hpp>

extern "C" {

// Read-only EcoNet FFI: blast-radius diagnostics per node.
char* econetgetblastradiusfornode(const char* dbpath, const char* nodeid);

// Read-only governance FFI: lane admissibility per shard.
char* econetlanegovernancecheck(const char* dbpath, const char* shardid);

// Shared free function for JSON buffers allocated by Rust.
void econetfreejson(char* ptr);

} // extern "C"

namespace ppx::waste::shredding::governance {

using nlohmann::json;

namespace {

std::string call_ffi_json(const char* (*fn)(const char*, const char*),
                          const std::string& db_path,
                          const std::string& key_arg) {
    std::string db = db_path;
    std::string arg = key_arg;

    char* c_db = const_cast<char*>(db.c_str());
    char* c_arg = const_cast<char*>(arg.c_str());

    char* raw = fn(c_db, c_arg);
    if (raw == nullptr) {
        throw std::runtime_error("FFI returned null JSON buffer");
    }

    std::string out(raw);
    econetfreejson(raw);
    return out;
}

KerSnapshot parse_ker_snapshot_from_blastradius_json(const json& j,
                                                     const std::string& machine_id) {
    KerSnapshot snapshot;

    snapshot.machine_id = machine_id;
    snapshot.region     = j.value("region", "");
    snapshot.lane       = j.value("lane", "");

    snapshot.carbon_radius       = j.value("carbon_radius", 0.0);
    snapshot.biodiversity_radius = j.value("biodiversity_radius", 0.0);

    snapshot.k_score = j.value("kscore", 0.0);
    snapshot.e_score = j.value("escore", 0.0);
    snapshot.r_score = j.value("rscore", 0.0);

    snapshot.vt_residual = j.value("vtresidual", 0.0);
    snapshot.roh_scalar  = j.value("rohscalar", 0.0);

    snapshot.ker_weighted_carbon_radius =
        j.value("kerweightedcarbonradius", snapshot.carbon_radius);
    snapshot.ker_weighted_biodiversity_radius =
        j.value("kerweightedbiodiversityradius", snapshot.biodiversity_radius);

    return snapshot;
}

} // namespace

BlastRadiusClient::BlastRadiusClient(const std::string& shared_lib_path)
    : lib_path_(shared_lib_path) {}

KerSnapshot BlastRadiusClient::fetch_ker_snapshot(const std::string& db_path,
                                                  const std::string& machine_id) const {
    std::string json_str = call_ffi_json(&econetgetblastradiusfornode, db_path, machine_id);
    json j = json::parse(json_str);
    return parse_ker_snapshot_from_blastradius_json(j, machine_id);
}

LaneGovernanceClient::LaneGovernanceClient(const std::string& shared_lib_path)
    : lib_path_(shared_lib_path) {}

LaneVerdict LaneGovernanceClient::fetch_lane_verdict(const std::string& db_path,
                                                     const std::string& shard_id) const {
    std::string json_str = call_ffi_json(&econetlanegovernancecheck, db_path, shard_id);
    json j = json::parse(json_str);

    LaneVerdict v{};
    v.admissible     = j.value("admissible", false);
    v.carbon_negative_ok = j.value("carbonnegativeok", false);
    v.restoration_ok     = j.value("restorationok", false);
    v.k_ok           = j.value("kok", false);
    v.e_ok           = j.value("eok", false);
    v.r_ok           = j.value("rok", false);
    v.roh_ok         = j.value("rohok", false);
    v.cyboquatic_ok  = j.value("cyboquaticok", false);
    v.reason         = j.value("reason", std::string{});

    return v;
}

ShreddingGovernanceAdapter::ShreddingGovernanceAdapter(const std::string& blastradius_lib_path,
                                                       const std::string& governance_lib_path)
    : blastradius_client_(blastradius_lib_path),
      lane_governance_client_(governance_lib_path) {}

ShreddingKerSnapshot ShreddingGovernanceAdapter::compute_snapshot(
    const std::string& db_path,
    const ShredderTelemetry& shredder,
    const ScreenDrumTelemetry& screen) const {

    KerSnapshot ker = blastradius_client_.fetch_ker_snapshot(db_path, shredder.machine_id);
    LaneVerdict lane = lane_governance_client_.fetch_lane_verdict(db_path, shredder.machine_id);

    ShreddingKerSnapshot out{};

    out.shredder = shredder;
    out.screen   = screen;
    out.ker      = ker;

    out.machine_id                       = ker.machine_id;
    out.region                           = ker.region;
    out.lane                             = ker.lane;
    out.carbon_radius                    = ker.carbon_radius;
    out.biodiversity_radius              = ker.biodiversity_radius;
    out.k_score                          = ker.k_score;
    out.e_score                          = ker.e_score;
    out.r_score                          = ker.r_score;
    out.vt_residual                      = ker.vt_residual;
    out.roh_scalar                       = ker.roh_scalar;
    out.ker_weighted_carbon_radius       = ker.ker_weighted_carbon_radius;
    out.ker_weighted_biodiversity_radius = ker.ker_weighted_biodiversity_radius;

    out.carbon_negative_ok = lane.carbon_negative_ok;
    out.restoration_ok     = lane.restoration_ok;

    out.lane_admissible    = lane.admissible &&
                             lane.carbon_negative_ok &&
                             lane.restoration_ok;
    out.lane_ker_ok        = lane.k_ok && lane.e_ok && lane.r_ok && lane.roh_ok;
    out.lane_cyboquatic_ok = lane.cyboquatic_ok;
    out.lane_reason        = lane.reason;

    out.shredding_safe_for_prod =
        out.lane_admissible && out.lane_ker_ok && out.lane_cyboquatic_ok;

    out.shredding_requires_restoration_focus =
        (!out.restoration_ok && lane.admissible) ||
        (out.restoration_ok && !out.carbon_negative_ok);

    return out;
}

} // namespace ppx::waste::shredding::governance
