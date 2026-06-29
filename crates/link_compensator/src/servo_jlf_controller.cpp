// crates/link_compensator/src/servo_jlf_controller.cpp

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

// Simple struct mirroring ServoJlfProfile2026v1 fields used at the edge.
struct ServoJlfProfile {
    std::string nodeid;
    std::string axisid;
    double r_tau_rawmax;
    double r_a_rawmax;
    double Jsafe;
    double Jwarn;
    double Jmax;
    double w_tau;
    double w_a;
    double alpha_clf;
    double alpha_cbf;
    double dt;
    double u_min;
    double u_max;
    double rho_clf;
    double rho_J;
    double expected_vt_delta;
    std::string corridorid;
    std::string kernelversion;
    std::string evidencehex;
};

// Telemetry row matching ServoJlfHealthCoord2026v1 CSV schema.
struct ServoJlfHealthSample {
    std::string nodeid;
    std::string axisid;
    double r_tau;
    double r_a;
    double Jk;
    double Jsafe;
    double Jwarn;
    double Jmax;
    double w_tau;
    double w_a;
    double vtbefore;
    double vtafter;
    std::string evidencehex;
};

// Placeholder for plant I/O (sensor + actuator) — to be wired to real HW.
struct PlantState {
    double torque_error;   // e_tau: residual between commanded and measured torque
    double anomaly_score;  // a_cnn: CNN anomaly output in raw units
    // Add joint position, velocity etc. as needed.
};

// ---- Utility helpers -------------------------------------------------------

static inline double clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Normalize torque residual and anomaly into [0,1] bands.
static inline double normalize_r(double raw, double rawmax) {
    if (rawmax <= 0.0) return 0.0;
    double v = std::fabs(raw) / rawmax;
    return clamp(v, 0.0, 1.0);
}

// ---- Profile loader --------------------------------------------------------

// Very simple CSV reader: expects one header line and many data lines.
// Returns the first row whose nodeid+axisid match local IDs.
std::optional<ServoJlfProfile> load_profile(
    const std::string &csv_path,
    const std::string &local_nodeid,
    const std::string &local_axisid
) {
    std::ifstream in(csv_path);
    if (!in) {
        std::cerr << "Failed to open profile CSV: " << csv_path << "\n";
        return std::nullopt;
    }

    std::string header;
    if (!std::getline(in, header)) {
        std::cerr << "Empty profile CSV\n";
        return std::nullopt;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string field;

        ServoJlfProfile p;

        std::getline(ss, p.nodeid, ',');
        std::getline(ss, p.axisid, ',');

        // These conversions assume the remaining columns are strictly ordered:
        // r_tau_rawmax,r_a_rawmax,Jsafe,Jwarn,Jmax,w_tau,w_a,
        // alpha_clf,alpha_cbf,dt,u_min,u_max,rho_clf,rho_J,
        // expected_vt_delta,corridorid,kernelversion,evidencehex.

        std::getline(ss, field, ','); p.r_tau_rawmax   = std::stod(field);
        std::getline(ss, field, ','); p.r_a_rawmax     = std::stod(field);
        std::getline(ss, field, ','); p.Jsafe          = std::stod(field);
        std::getline(ss, field, ','); p.Jwarn          = std::stod(field);
        std::getline(ss, field, ','); p.Jmax           = std::stod(field);
        std::getline(ss, field, ','); p.w_tau          = std::stod(field);
        std::getline(ss, field, ','); p.w_a            = std::stod(field);
        std::getline(ss, field, ','); p.alpha_clf      = std::stod(field);
        std::getline(ss, field, ','); p.alpha_cbf      = std::stod(field);
        std::getline(ss, field, ','); p.dt             = std::stod(field);
        std::getline(ss, field, ','); p.u_min          = std::stod(field);
        std::getline(ss, field, ','); p.u_max          = std::stod(field);
        std::getline(ss, field, ','); p.rho_clf        = std::stod(field);
        std::getline(ss, field, ','); p.rho_J          = std::stod(field);
        std::getline(ss, field, ','); p.expected_vt_delta = std::stod(field);
        std::getline(ss, p.corridorid, ',');
        std::getline(ss, p.kernelversion, ',');
        std::getline(ss, p.evidencehex, ',');

        if (p.nodeid == local_nodeid && p.axisid == local_axisid) {
            return p;
        }
    }

    std::cerr << "No profile row found for node=" << local_nodeid
              << " axis=" << local_axisid << "\n";
    return std::nullopt;
}

// ---- Plant I/O placeholders -----------------------------------------------

// TODO: Replace with real sensor reads from your Jetson / servo hardware.
PlantState read_plant_state() {
    PlantState s{};
    // Stub: replace with real measurements.
    s.torque_error   = 0.1;   // example value
    s.anomaly_score  = 0.0;   // assume healthy by default
    return s;
}

// TODO: Replace with real actuator write.
void write_torque_command(double u) {
    (void)u;
    // Send u to servo driver.
}

// ---- JLF + CLF–CBF update --------------------------------------------------

struct JlfControllerState {
    double Jk_prev{0.0};
    double vt_local{0.0};  // local Lyapunov surrogate
};

// Single control step: update r_tau, r_a, Jk, vt, and choose torque command u.
void control_step(
    const ServoJlfProfile &profile,
    JlfControllerState &state,
    const PlantState &plant,
    double &u_out,          // chosen torque command
    double &Jk_out,
    double &r_tau_out,
    double &r_a_out,
    double &vtbefore_out,
    double &vtafter_out
) {
    // Normalize residuals.
    double r_tau = normalize_r(plant.torque_error, profile.r_tau_rawmax);
    double r_a   = normalize_r(plant.anomaly_score, profile.r_a_rawmax);

    // Local Lyapunov surrogate before this step.
    double vt_before = state.vt_local;

    // Simple JLF update: CLF-like decay plus quadratic risk contribution.
    double Jk = profile.alpha_clf * state.Jk_prev
                + profile.w_tau * r_tau * r_tau
                + profile.w_a   * r_a   * r_a;
    Jk = clamp(Jk, 0.0, profile.Jmax);

    // Very simple CBF/CLF-inspired torque law (placeholder):
    // u = u_nominal - rho_J * dJ/d(e_tau), here approximated as -rho_J * r_tau.
    double u = -profile.rho_J * r_tau;
    u = clamp(u, profile.u_min, profile.u_max);

    // Local Lyapunov surrogate after this step.
    double vt_after = vt_before
                      + profile.w_tau * r_tau * r_tau
                      + profile.w_a   * r_a   * r_a;

    // Enforce local non-expansion for telemetry: if vt_after > vt_before, clamp.
    if (vt_after > vt_before) {
        vt_after = vt_before;
    }

    // Update internal state.
    state.Jk_prev  = Jk;
    state.vt_local = vt_after;

    // Write outputs.
    u_out          = u;
    Jk_out         = Jk;
    r_tau_out      = r_tau;
    r_a_out        = r_a;
    vtbefore_out   = vt_before;
    vtafter_out    = vt_after;
}

// ---- Telemetry writer ------------------------------------------------------

class HealthCsvWriter {
public:
    explicit HealthCsvWriter(const std::string &path)
        : path_(path), initialized_(false) {}

    void write_header_if_needed() {
        if (initialized_) return;
        std::ofstream out(path_, std::ios::app);
        if (!out) {
            std::cerr << "Failed to open health CSV for header: " << path_ << "\n";
            return;
        }
        out << "nodeid,axisid,r_tau,r_a,Jk,Jsafe,Jwarn,Jmax,"
               "w_tau,w_a,vtbefore,vtafter,evidencehex\n";
        initialized_ = true;
    }

    void append(const ServoJlfHealthSample &s) {
        write_header_if_needed();
        std::ofstream out(path_, std::ios::app);
        if (!out) {
            std::cerr << "Failed to open health CSV for append: " << path_ << "\n";
            return;
        }
        out << s.nodeid << ","
            << s.axisid << ","
            << s.r_tau << ","
            << s.r_a << ","
            << s.Jk << ","
            << s.Jsafe << ","
            << s.Jwarn << ","
            << s.Jmax << ","
            << s.w_tau << ","
            << s.w_a << ","
            << s.vtbefore << ","
            << s.vtafter << ","
            << s.evidencehex << "\n";
    }

private:
    std::string path_;
    bool initialized_;
};

// ---- Main control loop -----------------------------------------------------

int main(int argc, char **argv) {
    // Local IDs for this edge controller.
    const std::string local_nodeid = "NODE-JETSON-001";
    const std::string local_axisid = "AXIS-1";

    // Paths for profile and telemetry. Adjust to your actual layout.
    const std::string profile_csv_path =
        "titan-net/profiles/ServoJlfProfile2026v1.csv";
    const std::string health_csv_path =
        "titan-net/qpudatashards/ServoJlfHealthCoord2026v1.csv";

    // Load profile.
    auto profile_opt = load_profile(profile_csv_path, local_nodeid, local_axisid);
    if (!profile_opt) {
        return 1;
    }
    ServoJlfProfile profile = *profile_opt;

    // Initialize controller state and telemetry writer.
    JlfControllerState ctrl_state;
    HealthCsvWriter health_writer(health_csv_path);

    // Control and telemetry cadences.
    const double control_dt_sec = profile.dt;     // CLF–CBF loop period
    const double telemetry_dt_sec = 0.01;        // e.g., 100 Hz telemetry
    auto last_telemetry = std::chrono::steady_clock::now();

    while (true) {
        auto t_start = std::chrono::steady_clock::now();

        // 1. Read plant state.
        PlantState plant = read_plant_state();

        // 2. Run control step.
        double u = 0.0;
        double Jk = 0.0;
        double r_tau = 0.0;
        double r_a = 0.0;
        double vt_before = 0.0;
        double vt_after = 0.0;

        control_step(profile, ctrl_state, plant,
                     u, Jk, r_tau, r_a, vt_before, vt_after);

        // 3. Write torque command to actuator.
        write_torque_command(u);

        // 4. Periodic telemetry emission.
        auto now = std::chrono::steady_clock::now();
        double elapsed_telemetry =
            std::chrono::duration<double>(now - last_telemetry).count();
        if (elapsed_telemetry >= telemetry_dt_sec) {
            ServoJlfHealthSample sample;
            sample.nodeid     = local_nodeid;
            sample.axisid     = local_axisid;
            sample.r_tau      = clamp(r_tau, 0.0, 1.0);
            sample.r_a        = clamp(r_a, 0.0, 1.0);
            sample.Jk         = clamp(Jk, 0.0, profile.Jmax);
            sample.Jsafe      = profile.Jsafe;
            sample.Jwarn      = profile.Jwarn;
            sample.Jmax       = profile.Jmax;
            sample.w_tau      = profile.w_tau;
            sample.w_a        = profile.w_a;
            sample.vtbefore   = vt_before;
            sample.vtafter    = vt_after;
            sample.evidencehex = profile.evidencehex; // tie sample to profile+kernel

            // Basic invariant checks before writing (can be tightened later).
            if (sample.vtafter <= sample.vtbefore &&
                sample.Jwarn > sample.Jsafe &&
                sample.Jmax  > sample.Jwarn) {
                health_writer.append(sample);
            } else {
                std::cerr << "Sample violates local invariants, skipping CSV row\n";
            }

            last_telemetry = now;
        }

        // 5. Sleep until next control tick.
        auto t_end = std::chrono::steady_clock::now();
        double elapsed_control =
            std::chrono::duration<double>(t_end - t_start).count();
        double sleep_sec = control_dt_sec - elapsed_control;
        if (sleep_sec > 0.0) {
            std::this_thread::sleep_for(
                std::chrono::duration<double>(sleep_sec));
        }
    }

    return 0;
}
