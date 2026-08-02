// File: cpp/tools/phoenix_testbed_planner.cpp

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

struct PhaseAction {
    std::string label;
    std::string description;
};

struct PhasePlan {
    std::string name;
    std::vector<PhaseAction> actions;
};

static void print_phase(const PhasePlan& phase) {
    std::cout << "=== " << phase.name << " ===\n";
    for (std::size_t i = 0; i < phase.actions.size(); ++i) {
        const auto& a = phase.actions[i];
        std::cout << "  [" << (i + 1) << "] " << a.label << "\n";
        std::cout << "      " << a.description << "\n";
    }
    std::cout << "\n";
}

int main() {
    std::vector<PhasePlan> phases;

    PhasePlan phase0;
    phase0.name = "Phase 0 – Calibrate Evaluation Matrix Against Phoenix Data";
    phase0.actions.push_back({
        "Ingest DUSTIEAIM datasets",
        "Connect the evaluation stack to DUSTIEAIM heat flux, dust, and boundary-layer profiles. "
        "Use these as ground truth for physical_fidelity_index and numerical_robustness_index in the advection kernel, "
        "replacing placeholder values with Phoenix-specific measurements."
    });
    phase0.actions.push_back({
        "Compute initial component scores",
        "Run the ppx-eval-components crate (or the C++ equivalents) on real atmospheric input. "
        "Generate SevenDimProfile for advection, MARL, and streaming components and inspect KnowledgeFactor, EcoImpact, RiskOfHarm, "
        "Robustness, Sovereignty, EnergyEfficiency, and GovernanceAlignment."
    });
    phase0.actions.push_back({
        "Baseline governance ALN emission",
        "Execute ppx-governance-cli with all governance gates set to false. "
        "Produce phx_eligibility_gate_instance.aln that formalizes the current NotEligible status of the integrated Phoenix stack."
    });
    phase0.actions.push_back({
        "Publish eligibility roadmap",
        "Document and publish the conditions required to flip each governance gate to true: "
        "domain_performance_ok after DUSTIEAIM validation, safety_case_documented after stress-test review, "
        "sovereignty_compliant after signed data agreements, energy_neutral_or_renew after power audits, "
        "explainable_and_audited after explainability tooling is deployed."
    });
    phases.push_back(phase0);

    PhasePlan phase1;
    phase1.name = "Phase I – Minimal Physical Testbed with Single Control Loop";
    phase1.actions.push_back({
        "Deploy sensor–actuator micro-testbed",
        "Install 3–5 environmental sensor kits (air temperature, MRT, soil moisture, PM2.5) using LoRaWAN microcontrollers "
        "in a Phoenix community garden, schoolyard, or campus. Integrate 1–2 motorized shade sails or small evaporative misters "
        "controlled by a Raspberry Pi or similar edge node."
    });
    phase1.actions.push_back({
        "Integrate streaming pipeline",
        "Wire sensor data into the StreamingPipeline using MQTT. Enforce HeatSyncSLA and AirQualitySyncSLA with latency bounds "
        "derived from Phoenix urban heat island dynamics, keeping real-time control loops within safe response windows."
    });
    phase1.actions.push_back({
        "Deploy edge advection kernel",
        "Run a simplified 2-D finite-volume advection solver on the edge node. Calibrate against NOAA/NWS forecasts and local sensors, "
        "produce UrbanHeatEnvelope and AirQualityPlane fields that feed decisions and link back into the evaluation rubric."
    });
    phase1.actions.push_back({
        "Deploy single-agent MARL policy",
        "Train a single-agent MARL controller in simulation using the same observation spaces and action set as the physical testbed. "
        "On the edge node, gate every actuation through enforce_urban_climate_model_eligibility before applying commands to shade or misting hardware."
    });
    phase1.actions.push_back({
        "Daily governance self-evaluation",
        "Have the edge node run PhoenixStack::evaluate_system and ppx-governance-cli once per day. "
        "Emit updated phx_eligibility_gate_instance.aln with measured eco-impact. "
        "When statistical evidence shows MRT reduction without biodiversity loss or harmful side-effects, "
        "set domain_performance_ok = true via the governance config."
    });
    phases.push_back(phase1);

    PhasePlan phase2;
    phase2.name = "Phase II – Multi-Block Cool Corridor Pilot";
    phase2.actions.push_back({
        "Extend to multi-agent MARL",
        "Generalize the control loop to a multi-block Cool Corridor, coordinating shade, misting, and traffic routing across multiple cells. "
        "Use a shared Lyapunov function and explicit constraints for water rights, zoning, and grid limits to keep policies within safe envelopes."
    });
    phase2.actions.push_back({
        "Energy-aware edge operation",
        "Instrument each edge node with battery and solar telemetry. Include an energy_efficiency plane in MARL rewards so that "
        "total energy consumption is offset by local renewables, enabling energy_neutral_or_renew = true in governance evidence."
    });
    phase2.actions.push_back({
        "Federated training for sovereignty",
        "Implement federated MARL training so that raw data from tribal lands and sensitive neighborhoods never leaves local jurisdictions. "
        "Bind this to Indigenous Data Sovereignty agreements and, once signed, set sovereignty_compliant = true in SystemEvidence."
    });
    phase2.actions.push_back({
        "Formal safety case",
        "Run adversarial stress tests: sensor dropout, flash flood scenarios, rogue agent behaviors. "
        "Compile results into a formal safety case document and submit it to an external panel. "
        "Mark safety_case_documented = true only after review and approval."
    });
    phase2.actions.push_back({
        "Explainability and audit dashboard",
        "Build a web-based explainability dashboard (e.g., Rust Yew front end) showing why each MARL action was chosen, "
        "using interpretable features, SHAP values, or attention maps. "
        "When auditors and city officials can reliably understand and trace decisions, set explainable_and_audited = true."
    });
    phases.push_back(phase2);

    PhasePlan phase3;
    phase3.name = "Phase III – Transferability Toolkit and Replication";
    phase3.actions.push_back({
        "Parameterize city-specific context",
        "Move all city-bound variables (monsoon regime, canyon geometry, water rights, tribal agreements, zoning) "
        "into JSON or ALN scenario files consumed by AridCityTransferability. Keep PDE forms, MARL rules, and streaming abstractions city-independent."
    });
    phase3.actions.push_back({
        "Automated scoring for new cities",
        "Run the seven-dimension evaluation matrix on Tucson, Las Vegas, and Albuquerque archetypes. "
        "Emit eligibility instances (e.g., phx_eligibility_gate_instance_Tucson.aln) to transparently show how close each city is to eligibility."
    });
    phase3.actions.push_back({
        "Community-facing explainability",
        "Use the explainability dashboard and ALN reports to present adaptation results to local governments and citizen groups. "
        "Iterate with stakeholders to align MARL policies and streaming configurations with local governance expectations."
    });
    phases.push_back(phase3);

    PhasePlan immediate;
    immediate.name = "Immediate Next Actions (2–4 Weeks)";
    immediate.actions.push_back({
        "Procure and deploy sensor kits",
        "Purchase and install environmental sensor kits on a Phoenix university campus, connect them to the streaming pipeline, "
        "and begin ingesting live data."
    });
    immediate.actions.push_back({
        "Emit official baseline ALN",
        "Run ppx-governance-cli with current placeholder metrics to produce an official phx_eligibility_gate_instance.aln baseline. "
        "Publish this as the starting point for evidence-driven eligibility."
    });
    immediate.actions.push_back({
        "Implement edge advection kernel",
        "Start coding a simple 2-D advection kernel in Rust or C++, targeting Phoenix-relevant domains and calibrating against DUSTIEAIM and campus sensors."
    });
    immediate.actions.push_back({
        "Select and prepare physical intervention",
        "Choose a motorized shade or similar cooling intervention, order mechanical components and edge-compute modules, "
        "and design the wiring so it can be safely controlled by the MARL policy under governance gates."
    });
    phases.push_back(immediate);

    for (const auto& phase : phases) {
        print_phase(phase);
    }

    return 0;
}
