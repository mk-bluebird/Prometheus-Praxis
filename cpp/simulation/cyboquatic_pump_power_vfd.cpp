// File: cpp/simulation/cyboquatic_pump_power_vfd.cpp
#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <iomanip>

/**
 * Cyboquatic pump power model with variable frequency drive (VFD) efficiency curves.
 *
 * Base model:
 *   P = rho * g * Q * H / eta(Q),
 * where:
 *   rho: water density (kg/m^3),
 *   g: gravitational acceleration (m/s^2),
 *   Q: flow rate (m^3/s),
 *   H: head (m),
 *   eta(Q): effective motor efficiency, dependent on Q via VFD efficiency curve.
 *
 * We compute:
 *   energyreqJ = P * dt
 * for a sampling interval dt (s), and the partial derivative:
 *   ∂(energyreqJ) / ∂Q
 * needed for real-time carbon-optimising control.
 */

struct PumpState {
    double flow_rate_m3_s;
    double head_m;
    double vfd_efficiency;
    double power_W;
    double energyreqJ;
    double dEnergyreq_dFlow; // partial derivative ∂energyreqJ / ∂Q
};

class VfdEfficiencyCurve {
public:
    VfdEfficiencyCurve(double eta_peak,
                       double q_peak_ratio,
                       double eta_min)
        : eta_peak_(eta_peak),
          q_peak_ratio_(q_peak_ratio),
          eta_min_(eta_min)
    {}

    // Efficiency as function of q_ratio = Q / Q_rated.
    // Example quadratic curve centered at q_peak_ratio:
    //   eta(q) = max(eta_min, eta_peak - a * (q_ratio - q_peak_ratio)^2)
    // with a chosen so that efficiency decays realistically away from peak.
    double efficiency(double q_ratio) const {
        double a = eta_peak_ - eta_min_;
        double diff = q_ratio - q_peak_ratio_;
        double eta = eta_peak_ - a * diff * diff;
        if (eta < eta_min_) eta = eta_min_;
        if (eta > 1.0) eta = 1.0;
        return eta;
    }

    // Derivative d eta / d Q for given Q and Q_rated.
    double dEfficiency_dFlow(double Q, double Q_rated) const {
        double q_ratio = Q / Q_rated;
        double a = eta_peak_ - eta_min_;
        double diff = q_ratio - q_peak_ratio_;
        // eta(q_ratio) = eta_peak - a * diff^2
        // d eta / d q_ratio = -2 a * diff
        double dEta_dq = -2.0 * a * diff;
        // d q_ratio / d Q = 1 / Q_rated
        return dEta_dq / Q_rated;
    }

private:
    double eta_peak_;
    double q_peak_ratio_;
    double eta_min_;
};

class CyboquaticPumpModel {
public:
    CyboquaticPumpModel(double rho = 1000.0,
                        double g = 9.80665,
                        double Q_rated_m3_s = 0.2,
                        double eta_peak = 0.85,
                        double q_peak_ratio = 1.0,
                        double eta_min = 0.5)
        : rho_(rho),
          g_(g),
          Q_rated_(Q_rated_m3_s),
          vfd_curve_(eta_peak, q_peak_ratio, eta_min)
    {}

    PumpState compute_state(double Q_m3_s,
                            double H_m,
                            double dt_s) const
    {
        PumpState state{};
        state.flow_rate_m3_s = Q_m3_s;
        state.head_m = H_m;

        double q_ratio = Q_m3_s / Q_rated_;
        double eta = vfd_curve_.efficiency(q_ratio);
        state.vfd_efficiency = eta;

        // Power P(Q) = rho g Q H / eta(Q)
        double P = rho_ * g_ * Q_m3_s * H_m / eta;
        state.power_W = P;

        // energyreqJ = P * dt
        state.energyreqJ = P * dt_s;

        // Compute partial derivative d(energyreqJ)/dQ.
        // Let:
        //   P(Q) = A * Q / eta(Q), where A = rho g H.
        //   energyreqJ(Q) = dt * P(Q).
        // Then:
        //   dP/dQ = A * [ (1 / eta) - (Q / eta^2) * d eta/dQ ]
        //   d energyreqJ / dQ = dt * dP/dQ.
        double A = rho_ * g_ * H_m;
        double dEta_dQ = vfd_curve_.dEfficiency_dFlow(Q_m3_s, Q_rated_);
        double dP_dQ = A * ((1.0 / eta) - (Q_m3_s / (eta * eta)) * dEta_dQ);
        state.dEnergyreq_dFlow = dt_s * dP_dQ;

        return state;
    }

private:
    double rho_;
    double g_;
    double Q_rated_;
    VfdEfficiencyCurve vfd_curve_;
};

int main() {
    CyboquaticPumpModel model(
        1000.0,
        9.80665,
        0.2,   // Q_rated
        0.85,  // eta_peak
        1.0,   // q_peak_ratio
        0.5    // eta_min
    );

    double Q = 0.15;   // m^3/s
    double H = 4.0;    // m
    double dt = 60.0;  // s

    PumpState s = model.compute_state(Q, H, dt);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Flow rate Q (m^3/s): " << s.flow_rate_m3_s << "\n";
    std::cout << "Head H (m): " << s.head_m << "\n";
    std::cout << "VFD efficiency eta(Q): " << s.vfd_efficiency << "\n";
    std::cout << "Power P (W): " << s.power_W << "\n";
    std::cout << "Energy requirement (J): " << s.energyreqJ << "\n";
    std::cout << "d(energyreqJ)/dQ (J per (m^3/s)): " << s.dEnergyreq_dFlow << "\n";

    return 0;
}
