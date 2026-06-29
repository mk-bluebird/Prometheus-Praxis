#include "eco_state.hpp"
#include "eco_lyapunov.hpp"
#include "eco_corridor.hpp"
#include "eco_ringbuffer.hpp"

// Hardware-specific hooks (to be implemented per device)
EcoState2 read_state_from_sensors();
void apply_command_to_hardware(const EcoCommand& cmd);
uint32_t millis();
void telemetry_send(const EcoTelemetrySample& sample);

static EcoSetpoint2 g_ref{0.0f, 0.0f};
static LyapunovConfig2 g_vcfg{1.0f, 1.0f};
static Corridor2 g_corridor{-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f};
static float g_k1 = 1.0f;
static float g_k2 = 1.0f;

static EcoRingBuffer<256> g_ring;

void eco_control_step()
{
    EcoState2 x = read_state_from_sensors();

    const float V_prev = eco_compute_V(x, g_ref, g_vcfg);
    EcoCommand u_nom = eco_state_feedback(x, g_ref, g_k1, g_k2, g_corridor);
    EcoCommand u_safe = eco_corridor_override(x, g_corridor, u_nom);

    apply_command_to_hardware(u_safe);

    EcoState2 x_new = read_state_from_sensors();
    const float V_new = eco_compute_V(x_new, g_ref, g_vcfg);

    EcoTelemetrySample sample;
    sample.timestamp_ms = millis();
    sample.state = x_new;
    sample.command = u_safe;
    sample.V = V_new;
    g_ring.push(sample);

    (void)V_prev; // kept for optional V_new <= V_prev checks
}

void eco_telemetry_drain()
{
    EcoTelemetrySample s;
    while (!g_ring.empty()) {
        if (g_ring.pop(s)) {
            telemetry_send(s);
        }
    }
}
