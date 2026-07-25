
local ffi = require("ffi")

ffi.cdef[[
typedef struct {
    double canal_length_m;
    double canal_width_m;
    double upstream_flow_m3s;
    double surcharge_depth_m;
    double gate_open_fraction;
    double soil_cec_cmolkg;
    double bod_mgl;
    double tss_mgl;
    double vt_before;
    uint32_t hex_id;
} SurchargeEventInput;

typedef struct {
    double max_depth_downstream_m;
    double max_velocity_mps;
    double radius_overtop_m;
    double radius_scour_m;
    double pfos_risk_coord;
    double k_factor;
    double e_factor;
    double r_factor;
    uint32_t evidence_hex;
} BlastRadiusOutput;

int compute_blast_radius_flat(
    const void* in_buffer,
    void* out_buffer,
    size_t in_size,
    size_t out_size
);
]]

local lib = ffi.load("ecoengine_surcharge")  -- prebuilt .so

local M = {}

function M.compute_blast_radius(event)
    -- Allocate a single flat buffer for input + output.
    local in_t = ffi.new("SurchargeEventInput[1]")
    local out_t = ffi.new("BlastRadiusOutput[1]")

    in_t[0].canal_length_m       = event.canal_length_m
    in_t[0].canal_width_m        = event.canal_width_m
    in_t[0].upstream_flow_m3s    = event.upstream_flow_m3s
    in_t[0].surcharge_depth_m    = event.surcharge_depth_m
    in_t[0].gate_open_fraction   = event.gate_open_fraction
    in_t[0].soil_cec_cmolkg      = event.soil_cec_cmolkg
    in_t[0].bod_mgl              = event.bod_mgl
    in_t[0].tss_mgl              = event.tss_mgl
    in_t[0].vt_before            = event.vt_before or 0.0
    in_t[0].hex_id               = event.hex_id or 0

    local rc = lib.compute_blast_radius_flat(
        in_t,
        out_t,
        ffi.sizeof("SurchargeEventInput"),
        ffi.sizeof("BlastRadiusOutput")
    )
    if rc ~= 0 then
        return nil, ("ecoengine_surcharge error %d"):format(rc)
    end

    return {
        max_depth_downstream_m = tonumber(out_t[0].max_depth_downstream_m),
        max_velocity_mps       = tonumber(out_t[0].max_velocity_mps),
        radius_overtop_m       = tonumber(out_t[0].radius_overtop_m),
        radius_scour_m         = tonumber(out_t[0].radius_scour_m),
        pfos_risk_coord        = tonumber(out_t[0].pfos_risk_coord),
        k_factor               = tonumber(out_t[0].k_factor),
        e_factor               = tonumber(out_t[0].e_factor),
        r_factor               = tonumber(out_t[0].r_factor),
        evidence_hex           = tonumber(out_t[0].evidence_hex),
    }
end

return M
