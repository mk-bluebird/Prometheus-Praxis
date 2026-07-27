// filename: src/embedded/arm/modbus_gp_cbf.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (ARM-friendly, non-actuating)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

// Minimal POD view of a Modbus GP frame as seen by the embedded monitor.
// Real implementations will feed this from a separate Modbus stack.[file:37]
struct ModbusGpFrame {
    uint8_t  unit_id;       // Modbus slave/unit id
    uint8_t  function_code; // e.g. 0x03, 0x10
    uint16_t address;       // starting register/coil
    uint16_t length;        // number of registers/coils
    uint16_t crc;           // raw CRC as seen on the wire
    bool     crc_ok;        // decoded CRC check result
};

// Simple anomaly score in [0,1] for a single frame.
// This is a scalar proxy; ecosafety-core can interpret it as one
// coordinate in a larger risk vector.[file:37]
struct ModbusAnomalyScore {
    float score;        // 0..1, higher = more anomalous
    uint8_t severity;   // 0 = ok, 1 = warning, 2 = critical
};

// Cumulative barrier function state:
// h_t = Theta - A_t, where Theta is a fixed barrier level and
// A_t is cumulative anomaly mass over time.[file:37]
struct ModbusCbfState {
    float theta;        // barrier level (e.g. anomaly budget)
    float A_t;          // cumulative anomaly mass
    float h_t;          // current barrier value = theta - A_t
};

// Actuator gating decision for cyberphysical CBF guard.[file:37]
enum class CbfGateDecision : uint8_t {
    ALLOW = 0,      // actuator commands may pass
    DERATE = 1,     // allow only reduced/slow commands (Rust decides)
    BLOCK = 2       // block actuator commands until reset
};

// -----------------------------------------------------------------------------
// Anomaly scoring
// -----------------------------------------------------------------------------

// Frame-level anomaly: simple heuristic combining CRC failure,
// function code/length anomalies, and address range.[file:37]
static ModbusAnomalyScore score_modbus_frame(const ModbusGpFrame& f) {
    ModbusAnomalyScore out{};
    float s = 0.0f;

    // CRC failure is a strong anomaly.[file:37]
    if (!f.crc_ok) {
        s += 0.6f;
    }

    // Suspicious function codes (write multiple, diagnostics, etc.).[file:37]
    if (f.function_code == 0x10u ||  // Write Multiple Registers
        f.function_code == 0x0Fu ||  // Write Multiple Coils
        f.function_code == 0x08u) {  // Diagnostics
        s += 0.2f;
    }

    // Large writes are more anomalous than small probes.[file:37]
    if (f.length > 32u) {
        s += 0.2f;
    } else if (f.length > 8u) {
        s += 0.1f;
    }

    // Simple clipping and severity bands.[file:37]
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;

    out.score = s;
    if (s < 0.3f) {
        out.severity = 0; // ok
    } else if (s < 0.7f) {
        out.severity = 1; // warning
    } else {
        out.severity = 2; // critical
    }

    return out;
}

// -----------------------------------------------------------------------------
// Cumulative barrier function h_t = Theta - A_t
// -----------------------------------------------------------------------------

// Initialize CBF state with a given barrier Theta.[file:37]
static ModbusCbfState cbf_init(float theta) {
    ModbusCbfState st{};
    if (theta < 0.0f) theta = 0.0f;
    st.theta = theta;
    st.A_t   = 0.0f;
    st.h_t  = theta;
    return st;
}

// Update CBF state with a new anomaly score and time step.
// dt is a dimensionless step (e.g. 1 per frame); ecosafety-core can
// calibrate dt to real time if needed.[file:37]
static void cbf_update(ModbusCbfState& st,
                       const ModbusAnomalyScore& a,
                       float dt)
{
    if (dt < 0.0f) dt = 0.0f;

    // Simple accumulation: A_t+1 = A_t + score * dt.[file:37]
    st.A_t += a.score * dt;

    // h_t = Theta - A_t, clipped at zero.[file:37]
    st.h_t = st.theta - st.A_t;
    if (st.h_t < 0.0f) {
        st.h_t = 0.0f;
    }
}

// -----------------------------------------------------------------------------
// Cyberphysical gating thresholds
// -----------------------------------------------------------------------------

struct ModbusCbfThresholds {
    float h_block;    // below this, hard BLOCK
    float h_derate;   // below this, DERATE; above, ALLOW
};

// Decide actuator gate based on h_t and calibrated thresholds.
// Thresholds are chosen in Rust ecosafety-core and passed down;
// this function only applies the logic.[file:37]
static CbfGateDecision cbf_gate(const ModbusCbfState& st,
                                const ModbusCbfThresholds& th)
{
    float h = st.h_t;

    if (h <= th.h_block) {
        return CbfGateDecision::BLOCK;
    }
    if (h <= th.h_derate) {
        return CbfGateDecision::DERATE;
    }
    return CbfGateDecision::ALLOW;
}

// -----------------------------------------------------------------------------
// Public C API for embedded ARM integration
// -----------------------------------------------------------------------------

extern "C" {

// Initialize a CBF state given Theta.[file:37]
void modbus_cbf_init(float theta, ModbusCbfState* out_state) {
    if (!out_state) return;
    *out_state = cbf_init(theta);
}

// Score a single Modbus GP frame.[file:37]
void modbus_cbf_score_frame(const ModbusGpFrame* frame,
                            ModbusAnomalyScore* out_score)
{
    if (!frame || !out_score) return;
    *out_score = score_modbus_frame(*frame);
}

// Update CBF state with a scored frame and dt.[file:37]
void modbus_cbf_update(ModbusCbfState* state,
                       const ModbusAnomalyScore* score,
                       float dt)
{
    if (!state || !score) return;
    cbf_update(*state, *score, dt);
}

// Compute gate decision given current state and thresholds.[file:37]
CbfGateDecision modbus_cbf_gate(const ModbusCbfState* state,
                                const ModbusCbfThresholds* th)
{
    if (!state || !th) {
        return CbfGateDecision::BLOCK;
    }
    return cbf_gate(*state, *th);
}

} // extern "C"
