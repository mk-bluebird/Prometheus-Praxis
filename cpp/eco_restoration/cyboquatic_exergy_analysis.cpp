// File: cpp/eco_restoration/cyboquatic_exergy_analysis.cpp

#include <cmath>
#include <stdexcept>
#include <vector>

struct PumpAerationTelemetry {
    double flow_rate_m3s;    // Q
    double head_m;           // H
    double electrical_power_kw;
    double fluid_temperature_K;
    double ambient_temperature_K;
    double fluid_pressure_Pa;
    double ambient_pressure_Pa;
};

struct ExergyResult {
    double energy_kw;        // conventional energy rate
    double exergy_kw;        // exergy rate
    double exergy_destr_kw;  // exergy destruction rate
    double ker_ex;           // exergetic eco-impact factor
};

// Exergy analysis for pump-aeration system
class CyboquaticExergyAnalyzer {
public:
    ExergyResult compute(const PumpAerationTelemetry& t) const {
        // Energy: hydraulic power plus electrical usage
        double rho   = 1000.0;     // kg/m3
        double g     = 9.81;       // m/s2
        double Q     = t.flow_rate_m3s;
        double H     = t.head_m;
        double P_hyd = rho * g * Q * H / 1000.0; // kW

        double P_el  = t.electrical_power_kw;
        double energy_kw = P_el;

        // Exergy approximations (simplified):
        // Physical exergy per unit mass due to temperature difference:
        // ex_T ≈ cp * (T - T0) - cp * T0 * ln(T/T0)
        double cp = 4182.0; // J/kgK (water)
        double T  = t.fluid_temperature_K;
        double T0 = t.ambient_temperature_K;
        double ex_T = 0.0;
        if (T > 0.0 && T0 > 0.0) {
            ex_T = cp * ((T - T0) - T0 * std::log(T / T0));
        }

        // Pressure exergy per unit mass relative to ambient:
        // ex_P ≈ (P - P0) / (rho)
        double P    = t.fluid_pressure_Pa;
        double P0   = t.ambient_pressure_Pa;
        double ex_P = (P - P0) / rho;

        // Exergy rate (kW) from hydraulic plus thermo-mechanical effects:
        double ex_specific_Jkg = ex_T + ex_P;
        double mass_flow_kg_s  = rho * Q;
        double ex_rate_kw      = P_hyd + ex_specific_Jkg * mass_flow_kg_s / 1000.0;

        // Exergy destruction: electrical input minus useful exergy rate
        double ex_destr_kw = std::max(0.0, P_el - ex_rate_kw);

        // Exergetic eco-impact ker_ex: normalized exergy destruction
        double ker_ex = normalizeExergyDestruction(ex_destr_kw);

        ExergyResult res;
        res.energy_kw       = energy_kw;
        res.exergy_kw       = ex_rate_kw;
        res.exergy_destr_kw = ex_destr_kw;
        res.ker_ex          = ker_ex;
        return res;
    }

    // Gradient of efficiency with respect to control parameters (e.g., motor speed u)
    // modified by exergetic eco-impact; here we only sketch a derivative w.r.t u.
    double efficiencyGradient(double u,
                              const PumpAerationTelemetry& t) const {
        // Base efficiency gradient (toy model): dη/du
        double eta = motorEfficiency(u);
        double dEta_du = baseEfficiencyDerivative(u);

        // Exergy destruction sensitivity: penalize regions where ex_destr_kw is high
        ExergyResult ex = compute(t);
        double penalty = ex.ker_ex; // higher ker_ex => stronger penalty

        // Modified gradient: push optimization away from high-exergy destruction regimes
        double modifiedGradient = dEta_du - lambda_ * penalty;
        return modifiedGradient;
    }

private:
    double lambda_ = 0.1; // tuning parameter for exergy penalty

    double normalizeExergyDestruction(double ex_destr_kw) const {
        // Map exergy destruction to [0,1] risk coordinate
        double ex_ref = 10.0; // reference exergy (kW) for normalization
        double r = ex_destr_kw / ex_ref;
        if (r < 0.0) r = 0.0;
        if (r > 1.0) r = 1.0;
        return r;
    }

    double motorEfficiency(double u) const {
        // Simple quadratic efficiency curve: peak at u = 0.7
        double u_peak = 0.7;
        double eta_min = 0.5;
        double eta_max = 0.9;
        double alpha   = 10.0;
        double diff    = u - u_peak;
        double eta     = eta_min + (eta_max - eta_min) * std::exp(-alpha * diff * diff);
        return eta;
    }

    double baseEfficiencyDerivative(double u) const {
        // Derivative of efficiency curve w.r.t u
        double u_peak = 0.7;
        double eta_min = 0.5;
        double eta_max = 0.9;
        double alpha   = 10.0;
        double diff    = u - u_peak;
        double common  = (eta_max - eta_min) * std::exp(-alpha * diff * diff);
        double dEta_du = common * (-2.0 * alpha * diff);
        return dEta_du;
    }
};
