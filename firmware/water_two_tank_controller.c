// Filename: firmware/water_two_tank_controller.c
// Destination: eco_restoration_shard/firmware/water_two_tank_controller.c
//
// C control loop implementing a Lyapunov-consistent two-tank, one-pump controller
// with safety corridors and telemetry suitable for ingestion by the Rust
// water_telemetry_ingest crate.
//
// This file is intended for bare-metal MCU / PLC environments.
// Hardware-specific bindings for read/write functions must be provided elsewhere.

#include <stdint.h>
#include <stdbool.h>

// ---------- Configuration ----------

// Desired steady levels (meters)
static const float H1_REF = 1.0f;   // Tank 1 target level
static const float H2_REF = 0.8f;   // Tank 2 target level

// Pump command limits (normalized 0.0 .. 1.0)
static const float U_MIN = 0.0f;
static const float U_MAX = 1.0f;

// Operating point pump command (to maintain steady levels)
static const float U_REF = 0.5f;    // chosen from hydraulic calibration

// State-feedback gains (Lyapunov-consistent, chosen offline)
static const float K1 = -0.8f;      // feedback on Tank 1 deviation
static const float K2 = -0.3f;      // feedback on Tank 2 deviation

// Level safety corridors (meters)
static const float H1_MIN = 0.3f;
static const float H1_MAX = 1.5f;
static const float H2_MIN = 0.2f;
static const float H2_MAX = 1.2f;

// Basic sensor validity thresholds (meters)
static const float H_MIN_VALID = 0.0f;
static const float H_MAX_VALID = 2.0f;

// Lyapunov weights
static const float P1 = 1.0f;  // weight on Tank 1 deviation
static const float P2 = 1.0f;  // weight on Tank 2 deviation

// ---------- Hardware abstraction layer ----------

// These functions must be implemented to bind to actual I/O.
// For example, they may read from ADC channels and write to PWM or analog outputs.

float read_tank1_level_m(void);
float read_tank2_level_m(void);
void write_pump_command(float u_norm);

// Logging hooks for warnings and alarms (can be mapped to UART, LEDs, etc.)
void log_warning(const char *msg);
void log_alarm(const char *msg);

// Retrieve current UTC timestamp in seconds for telemetry
int64_t get_timestamp_utc_s(void);

// Retrieve node identifier string for this controller (e.g., from config)
const char *get_node_id(void);

// ---------- Telemetry ring buffer ----------

typedef struct {
    float h1_m;
    float h2_m;
    float v_lyapunov;
    float u_cmd_norm;
    int64_t timestamp_utc_s;
} telemetry_sample_t;

#define TELEMETRY_BUF_SIZE 256

#if (TELEMETRY_BUF_SIZE & (TELEMETRY_BUF_SIZE - 1)) != 0
#error "TELEMETRY_BUF_SIZE must be power of two"
#endif

typedef struct {
    telemetry_sample_t buf[TELEMETRY_BUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t mask;
} telemetry_ring_t;

static telemetry_ring_t telemetry_rb = {
    .head = 0U,
    .tail = 0U,
    .mask = TELEMETRY_BUF_SIZE - 1U
};

static void telemetry_push(telemetry_sample_t sample)
{
    uint32_t head = telemetry_rb.head;
    uint32_t next_head = head + 1U;
    uint32_t tail = telemetry_rb.tail;

    if ((next_head - tail) > TELEMETRY_BUF_SIZE) {
        telemetry_rb.tail = tail + 1U;
    }

    telemetry_rb.buf[head & telemetry_rb.mask] = sample;

#if defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#endif

    telemetry_rb.head = next_head;
}

bool telemetry_pop(telemetry_sample_t *out)
{
    uint32_t tail = telemetry_rb.tail;
    uint32_t head = telemetry_rb.head;

    if (tail == head) {
        return false;
    }

    *out = telemetry_rb.buf[tail & telemetry_rb.mask];

#if defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#endif

    telemetry_rb.tail = tail + 1U;
    return true;
}

// ---------- Helpers ----------

static float clip(float x, float min_val, float max_val)
{
    if (x < min_val) {
        return min_val;
    }
    if (x > max_val) {
        return max_val;
    }
    return x;
}

static bool level_is_valid(float h)
{
    return (h >= H_MIN_VALID) && (h <= H_MAX_VALID);
}

static float compute_lyapunov(float h1_tilde, float h2_tilde)
{
    return P1 * h1_tilde * h1_tilde + P2 * h2_tilde * h2_tilde;
}

// ---------- Core control step ----------

void two_tank_control_step(void)
{
    float h1 = read_tank1_level_m();
    float h2 = read_tank2_level_m();

    int64_t ts = get_timestamp_utc_s();

    if (!level_is_valid(h1) || !level_is_valid(h2)) {
        log_alarm("Tank level sensor invalid");
        write_pump_command(U_MIN);

        telemetry_sample_t sample = {
            .h1_m = h1,
            .h2_m = h2,
            .v_lyapunov = 0.0f,
            .u_cmd_norm = U_MIN,
            .timestamp_utc_s = ts
        };
        telemetry_push(sample);
        return;
    }

    float h1_tilde = h1 - H1_REF;
    float h2_tilde = h2 - H2_REF;

    float V = compute_lyapunov(h1_tilde, h2_tilde);

    float u_tilde = K1 * h1_tilde + K2 * h2_tilde;
    float u_cmd = U_REF + u_tilde;

    if (h1 < H1_MIN) {
        log_warning("Tank 1 below minimum; boosting pump");
        u_cmd = clip(U_REF + 0.2f, U_MIN, U_MAX);
    }
    if (h1 > H1_MAX) {
        log_warning("Tank 1 above maximum; reducing pump");
        u_cmd = clip(U_REF - 0.3f, U_MIN, U_MAX);
    }
    if (h2 < H2_MIN) {
        log_warning("Tank 2 below minimum; ensuring inflow");
        u_cmd = clip(U_REF + 0.1f, U_MIN, U_MAX);
    }
    if (h2 > H2_MAX) {
        log_warning("Tank 2 above maximum; reducing pump");
        u_cmd = clip(U_REF - 0.2f, U_MIN, U_MAX);
    }

    u_cmd = clip(u_cmd, U_MIN, U_MAX);

    write_pump_command(u_cmd);

    telemetry_sample_t sample = {
        .h1_m = h1,
        .h2_m = h2,
        .v_lyapunov = V,
        .u_cmd_norm = u_cmd,
        .timestamp_utc_s = ts
    };
    telemetry_push(sample);
}

// ---------- Telemetry export ----------

//
// Example: serialize telemetry samples to a binary/JSON stream.
// The Rust water_telemetry_ingest crate expects WaterTankTelemetryV1 records,
// which correspond directly to telemetry_sample_t plus node_id.
//

void export_telemetry_samples(void)
{
    telemetry_sample_t sample;
    const char *node_id = get_node_id();

    while (telemetry_pop(&sample)) {
        // Serialize (node_id, sample.timestamp_utc_s, sample.h1_m, sample.h2_m,
        //            sample.v_lyapunov, sample.u_cmd_norm) to the chosen transport.
        // The exact format (JSON, CBOR, protobuf) and transport (UART, TCP, etc.)
        // is left to the platform-specific implementation.
        (void)node_id;
        (void)sample;
        // Implement platform-specific serialization and send here.
    }
}
