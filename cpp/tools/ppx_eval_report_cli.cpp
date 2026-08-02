// File: cpp/tools/ppx_eval_report_cli.cpp

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>

namespace ppx {

enum class Dimension {
    KnowledgeFactor,
    EcoImpact,
    RiskOfHarm,
    Robustness,
    Sovereignty,
    EnergyEfficiency,
    GovernanceAlignment
};

struct Score {
    double value;

    Score() : value(0.0) {}
    explicit Score(double v) : value(v) {
        clamp();
    }

    void clamp() {
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;
    }

    bool is_min(double min) const {
        return value >= min;
    }
};

struct SevenDimProfile {
    Score knowledge_factor;
    Score eco_impact;
    Score risk_of_harm;
    Score robustness;
    Score sovereignty;
    Score energy_efficiency;
    Score governance_alignment;

    Score min_score() const {
        double m = knowledge_factor.value;
        m = std::min(m, eco_impact.value);
        m = std::min(m, risk_of_harm.value);
        m = std::min(m, robustness.value);
        m = std::min(m, sovereignty.value);
        m = std::min(m, energy_efficiency.value);
        m = std::min(m, governance_alignment.value);
        return Score(m);
    }

    bool satisfies_threshold(double per_dim_min) const {
        return knowledge_factor.is_min(per_dim_min)
            && eco_impact.is_min(per_dim_min)
            && risk_of_harm.is_min(per_dim_min)
            && robustness.is_min(per_dim_min)
            && sovereignty.is_min(per_dim_min)
            && energy_efficiency.is_min(per_dim_min)
            && governance_alignment.is_min(per_dim_min);
    }
};

struct PhoenixThresholds {
    double component_min;
    double system_min;
    double max_risk_of_harm;

    static PhoenixThresholds default_thresholds() {
        PhoenixThresholds t{};
        t.component_min = 0.75;
        t.system_min = 0.80;
        t.max_risk_of_harm = 0.25;
        return t;
    }
};

class ComponentEvaluable {
public:
    virtual ~ComponentEvaluable() = default;
    virtual const std::string& id() const = 0;
    virtual SevenDimProfile evaluate_component() const = 0;
};

struct PhoenixContext {
    double monsoon_intensity_index;
    double canyon_heat_gradient;
    double fog_channel_density;
    double industrial_waste_load;
    double sovereignty_weight;
    double energy_constraint;

    static PhoenixContext phoenix_default() {
        PhoenixContext ctx{};
        ctx.monsoon_intensity_index = 0.85;
        ctx.canyon_heat_gradient = 0.90;
        ctx.fog_channel_density = 0.75;
        ctx.industrial_waste_load = 0.80;
        ctx.sovereignty_weight = 0.95;
        ctx.energy_constraint = 0.70;
        return ctx;
    }
};

class AdvectionKernel : public ComponentEvaluable {
public:
    std::string scheme_name;
    double cfl_safety_margin;
    double physical_fidelity_index;
    double restored_flow_ratio;
    double numerical_robustness_index;
    PhoenixContext ctx;

    AdvectionKernel(
        const std::string& scheme,
        double cfl_margin,
        double fidelity,
        double restored_flow,
        double robustness,
        const PhoenixContext& context
    )
        : scheme_name(scheme),
          cfl_safety_margin(cfl_margin),
          physical_fidelity_index(fidelity),
          restored_flow_ratio(restored_flow),
          numerical_robustness_index(robustness),
          ctx(context),
          id_str("advection_kernel")
    {}

    const std::string& id() const override {
        return id_str;
    }

    SevenDimProfile evaluate_component() const override {
        SevenDimProfile p{};

        double fidelity = clamp01(physical_fidelity_index);
        double cfl = clamp01(cfl_safety_margin);
        double restored = clamp01(restored_flow_ratio);
        double num_rob = clamp01(numerical_robustness_index);
        double fog = clamp01(ctx.fog_channel_density);
        double monsoon = clamp01(ctx.monsoon_intensity_index);
        double sov = clamp01(ctx.sovereignty_weight);
        double energy = clamp01(ctx.energy_constraint);

        p.knowledge_factor = Score(0.5 + 0.5 * fidelity);

        p.eco_impact = Score((restored * 0.7 + fog * 0.3));

        p.risk_of_harm = Score(
            (1.0 - cfl) * 0.5 +
            (1.0 - num_rob) * 0.5
        );

        p.robustness = Score(
            0.5 * num_rob +
            0.5 * (1.0 - monsoon * 0.2)
        );

        p.sovereignty = Score(
            0.8 * sov +
            0.2 * fidelity
        );

        p.energy_efficiency = Score(
            (1.0 - energy) * 0.7 +
            0.3 * num_rob
        );

        p.governance_alignment = Score(
            0.7 * sov +
            0.3 * fidelity
        );

        p.knowledge_factor.clamp();
        p.eco_impact.clamp();
        p.risk_of_harm.clamp();
        p.robustness.clamp();
        p.sovereignty.clamp();
        p.energy_efficiency.clamp();
        p.governance_alignment.clamp();

        return p;
    }

private:
    std::string id_str;

    static double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }
};

class MarlArchitecture : public ComponentEvaluable {
public:
    double policy_alignment_index;
    double rogue_pattern_resilience;
    double multi_actor_scalability;
    double consent_corridor_strength;
    double cybercore_binding_strength;
    PhoenixContext ctx;

    MarlArchitecture(
        double policy_align,
        double rogue_res,
        double scalability,
        double consent_strength,
        double cybercore_strength,
        const PhoenixContext& context
    )
        : policy_alignment_index(policy_align),
          rogue_pattern_resilience(rogue_res),
          multi_actor_scalability(scalability),
          consent_corridor_strength(consent_strength),
          cybercore_binding_strength(cybercore_strength),
          ctx(context),
          id_str("marl_architecture")
    {}

    const std::string& id() const override {
        return id_str;
    }

    SevenDimProfile evaluate_component() const override {
        SevenDimProfile p{};

        double policy = clamp01(policy_alignment_index);
        double rogue = clamp01(rogue_pattern_resilience);
        double multi = clamp01(multi_actor_scalability);
        double consent = clamp01(consent_corridor_strength);
        double core = clamp01(cybercore_binding_strength);
        double energy = clamp01(ctx.energy_constraint);

        p.knowledge_factor = Score(
            0.5 * policy +
            0.5 * multi
        );

        p.eco_impact = Score(
            0.6 * policy +
            0.4 * rogue
        );

        p.risk_of_harm = Score(
            (1.0 - rogue) * 0.6 +
            (1.0 - consent) * 0.4
        );

        p.robustness = Score(
            0.5 * rogue +
            0.5 * multi
        );

        p.sovereignty = Score(
            0.5 * consent +
            0.5 * core
        );

        p.energy_efficiency = Score(
            (1.0 - energy) * 0.5 +
            0.5 * multi
        );

        p.governance_alignment = Score(
            0.7 * core +
            0.3 * policy
        );

        p.knowledge_factor.clamp();
        p.eco_impact.clamp();
        p.risk_of_harm.clamp();
        p.robustness.clamp();
        p.sovereignty.clamp();
        p.energy_efficiency.clamp();
        p.governance_alignment.clamp();

        return p;
    }

private:
    std::string id_str;

    static double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }
};

class StreamingPipeline : public ComponentEvaluable {
public:
    double end_to_end_latency_ms;
    double failure_recovery_index;
    double data_sovereignty_index;
    double energy_cost_per_event;
    double biosignal_integration_index;
    PhoenixContext ctx;

    StreamingPipeline(
        double latency_ms,
        double failure_recovery,
        double data_sov,
        double energy_cost,
        double biosignal_integration,
        const PhoenixContext& context
    )
        : end_to_end_latency_ms(latency_ms),
          failure_recovery_index(failure_recovery),
          data_sovereignty_index(data_sov),
          energy_cost_per_event(energy_cost),
          biosignal_integration_index(biosignal_integration),
          ctx(context),
          id_str("streaming_pipeline")
    {}

    const std::string& id() const override {
        return id_str;
    }

    SevenDimProfile evaluate_component() const override {
        SevenDimProfile p{};

        double latency_score = 1.0 - clamp01(end_to_end_latency_ms / 1000.0);
        double failure = clamp01(failure_recovery_index);
        double data_sov = clamp01(data_sovereignty_index);
        double energy_cost = clamp01(energy_cost_per_event);
        double bio = clamp01(biosignal_integration_index);
        double energy = clamp01(ctx.energy_constraint);

        p.knowledge_factor = Score(
            0.5 * latency_score +
            0.5 * bio
        );

        p.eco_impact = Score(
            (1.0 - energy_cost) * 0.6 +
            0.4 * failure
        );

        p.risk_of_harm = Score(
            (1.0 - failure) * 0.5 +
            (1.0 - data_sov) * 0.5
        );

        p.robustness = Score(
            0.6 * failure +
            0.4 * latency_score
        );

        p.sovereignty = Score(data_sov);

        p.energy_efficiency = Score(
            (1.0 - energy_cost) * 0.8 +
            0.2 * energy
        );

        p.governance_alignment = Score(
            0.6 * data_sov +
            0.4 * failure
        );

        p.knowledge_factor.clamp();
        p.eco_impact.clamp();
        p.risk_of_harm.clamp();
        p.robustness.clamp();
        p.sovereignty.clamp();
        p.energy_efficiency.clamp();
        p.governance_alignment.clamp();

        return p;
    }

private:
    std::string id_str;

    static double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }
};

struct SystemEligibility {
    SevenDimProfile profile;
    bool eligible;
    std::string notes;
};

class PhoenixStack {
public:
    PhoenixStack(
        const AdvectionKernel& adv_,
        const MarlArchitecture& marl_,
        const StreamingPipeline& stream_,
        const PhoenixThresholds& thresholds_
    )
        : adv(adv_),
          marl(marl_),
          stream(stream_),
          thresholds(thresholds_)
    {}

    SystemEligibility evaluate_system(const std::vector<const ComponentEvaluable*>& components) const {
        std::ostringstream notes;
        for (const auto* c : components) {
            SevenDimProfile profile = c->evaluate_component();
            if (!profile.satisfies_threshold(thresholds.component_min)) {
                notes << "Component '" << c->id() << "' failed per-dimension component_min.\n";
            }
            if (profile.risk_of_harm.value > thresholds.max_risk_of_harm) {
                notes << "Component '" << c->id() << "' exceeded max_risk_of_harm.\n";
            }
        }

        SevenDimProfile adv_p = adv.evaluate_component();
        SevenDimProfile marl_p = marl.evaluate_component();
        SevenDimProfile stream_p = stream.evaluate_component();

        SevenDimProfile aggregated{};
        aggregated.knowledge_factor = Score(
            (adv_p.knowledge_factor.value +
             marl_p.knowledge_factor.value +
             stream_p.knowledge_factor.value) / 3.0
        );
        aggregated.eco_impact = Score(
            (adv_p.eco_impact.value +
             marl_p.eco_impact.value +
             stream_p.eco_impact.value) / 3.0
        );
        aggregated.risk_of_harm = Score(
            std::max(
                adv_p.risk_of_harm.value,
                std::max(marl_p.risk_of_harm.value, stream_p.risk_of_harm.value)
            )
        );
        aggregated.robustness = Score(
            (adv_p.robustness.value +
             marl_p.robustness.value +
             stream_p.robustness.value) / 3.0
        );
        aggregated.sovereignty = Score(
            (adv_p.sovereignty.value +
             marl_p.sovereignty.value +
             stream_p.sovereignty.value) / 3.0
        );
        aggregated.energy_efficiency = Score(
            (adv_p.energy_efficiency.value +
             marl_p.energy_efficiency.value +
             stream_p.energy_efficiency.value) / 3.0
        );
        aggregated.governance_alignment = Score(
            (adv_p.governance_alignment.value +
             marl_p.governance_alignment.value +
             stream_p.governance_alignment.value) / 3.0
        );

        bool eligible =
            aggregated.satisfies_threshold(thresholds.system_min) &&
            aggregated.risk_of_harm.value <= thresholds.max_risk_of_harm &&
            notes.str().empty();

        if (!eligible) {
            notes << "Integrated stack failed Phoenix eligibility thresholds.\n";
        } else {
            notes << "Integrated stack passes Phoenix eligibility thresholds.\n";
        }

        SystemEligibility e{};
        e.profile = aggregated;
        e.eligible = eligible;
        e.notes = notes.str();
        return e;
    }

private:
    AdvectionKernel adv;
    MarlArchitecture marl;
    StreamingPipeline stream;
    PhoenixThresholds thresholds;
};

static std::string dimension_name(Dimension d) {
    switch (d) {
        case Dimension::KnowledgeFactor:     return "KnowledgeFactor";
        case Dimension::EcoImpact:          return "EcoImpact";
        case Dimension::RiskOfHarm:         return "RiskOfHarm";
        case Dimension::Robustness:         return "Robustness";
        case Dimension::Sovereignty:        return "Sovereignty";
        case Dimension::EnergyEfficiency:   return "EnergyEfficiency";
        case Dimension::GovernanceAlignment:return "GovernanceAlignment";
        default:                            return "Unknown";
    }
}

static std::vector<std::pair<Dimension, double>> profile_to_rows(const SevenDimProfile& p) {
    std::vector<std::pair<Dimension, double>> rows;
    rows.emplace_back(Dimension::KnowledgeFactor,     p.knowledge_factor.value);
    rows.emplace_back(Dimension::EcoImpact,          p.eco_impact.value);
    rows.emplace_back(Dimension::RiskOfHarm,         p.risk_of_harm.value);
    rows.emplace_back(Dimension::Robustness,         p.robustness.value);
    rows.emplace_back(Dimension::Sovereignty,        p.sovereignty.value);
    rows.emplace_back(Dimension::EnergyEfficiency,   p.energy_efficiency.value);
    rows.emplace_back(Dimension::GovernanceAlignment,p.governance_alignment.value);
    return rows;
}

struct JsonComponentRow {
    std::string id;
    SevenDimProfile profile;
};

static std::string json_escape(const std::string& s) {
    std::ostringstream oss;
    oss << '"';
    for (char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u"
                        << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    oss << c;
                }
        }
    }
    oss << '"';
    return oss.str();
}

static std::string json_profile(const SevenDimProfile& p) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"knowledge_factor\":" << p.knowledge_factor.value << ",";
    oss << "\"eco_impact\":" << p.eco_impact.value << ",";
    oss << "\"risk_of_harm\":" << p.risk_of_harm.value << ",";
    oss << "\"robustness\":" << p.robustness.value << ",";
    oss << "\"sovereignty\":" << p.sovereignty.value << ",";
    oss << "\"energy_efficiency\":" << p.energy_efficiency.value << ",";
    oss << "\"governance_alignment\":" << p.governance_alignment.value;
    oss << "}";
    return oss.str();
}

static std::string json_component_matrix(
    const std::vector<JsonComponentRow>& rows
) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"components\":[";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        oss << "{";
        oss << "\"id\":" << json_escape(r.id) << ",";
        oss << "\"profile\":" << json_profile(r.profile);
        oss << "}";
        if (i + 1 < rows.size()) {
            oss << ",";
        }
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

static std::string json_eligibility(const SystemEligibility& e) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"eligible\":" << (e.eligible ? "true" : "false") << ",";
    oss << "\"profile\":" << json_profile(e.profile) << ",";
    oss << "\"notes\":" << json_escape(e.notes);
    oss << "}";
    return oss.str();
}

static std::string aln_report(
    const std::vector<JsonComponentRow>& rows,
    const SystemEligibility& elig
) {
    std::ostringstream oss;
    oss << "module PhoenixEvalReport {\n";

    oss << "  components {\n";
    for (const auto& r : rows) {
        oss << "    component " << r.id << " {\n";
        oss << "      KnowledgeFactor     = " << r.profile.knowledge_factor.value << ";\n";
        oss << "      EcoImpact           = " << r.profile.eco_impact.value << ";\n";
        oss << "      RiskOfHarm          = " << r.profile.risk_of_harm.value << ";\n";
        oss << "      Robustness          = " << r.profile.robustness.value << ";\n";
        oss << "      Sovereignty         = " << r.profile.sovereignty.value << ";\n";
        oss << "      EnergyEfficiency    = " << r.profile.energy_efficiency.value << ";\n";
        oss << "      GovernanceAlignment = " << r.profile.governance_alignment.value << ";\n";
        oss << "    }\n";
    }
    oss << "  }\n";

    oss << "  system {\n";
    oss << "    Eligible = " << (elig.eligible ? "true" : "false") << ";\n";
    oss << "    KnowledgeFactor     = " << elig.profile.knowledge_factor.value << ";\n";
    oss << "    EcoImpact           = " << elig.profile.eco_impact.value << ";\n";
    oss << "    RiskOfHarm          = " << elig.profile.risk_of_harm.value << ";\n";
    oss << "    Robustness          = " << elig.profile.robustness.value << ";\n";
    oss << "    Sovereignty         = " << elig.profile.sovereignty.value << ";\n";
    oss << "    EnergyEfficiency    = " << elig.profile.energy_efficiency.value << ";\n";
    oss << "    GovernanceAlignment = " << elig.profile.governance_alignment.value << ";\n";
    oss << "    Notes               = \"" << elig.notes << "\";\n";
    oss << "  }\n";

    oss << "}\n";
    return oss.str();
}

static void print_ascii_matrix(const std::vector<JsonComponentRow>& rows) {
    std::cout << "=== Prometheus-Praxis Component Evaluation Matrix (Phoenix, C++) ===\n\n";
    std::cout << std::left << std::setw(20) << "Component" << " | "
              << std::right << std::setw(8)  << "Know"
              << std::setw(8)  << "Eco"
              << std::setw(8)  << "Risk"
              << std::setw(10) << "Robust"
              << std::setw(12) << "Sovereign"
              << std::setw(10) << "Energy"
              << std::setw(20) << "Governance" << "\n";

    std::cout << std::string(20 + 3 + 8 * 3 + 10 + 12 + 10 + 20, '-') << "\n";

    for (const auto& r : rows) {
        const SevenDimProfile& p = r.profile;
        std::cout << std::left << std::setw(20) << r.id << " | "
                  << std::right << std::setw(8)  << std::fixed << std::setprecision(3) << p.knowledge_factor.value
                  << std::setw(8)  << p.eco_impact.value
                  << std::setw(8)  << p.risk_of_harm.value
                  << std::setw(10) << p.robustness.value
                  << std::setw(12) << p.sovereignty.value
                  << std::setw(10) << p.energy_efficiency.value
                  << std::setw(20) << p.governance_alignment.value
                  << "\n";
    }

    std::cout << std::string(20 + 3 + 8 * 3 + 10 + 12 + 10 + 20, '-') << "\n";
}

static void write_file(const std::string& path, const std::string& content) {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << "\n";
        return;
    }
    ofs << content;
    ofs.close();
}

} // namespace ppx

int main(int argc, char** argv) {
    using namespace ppx;

    // Phoenix context and components.
    PhoenixContext ctx = PhoenixContext::phoenix_default();

    AdvectionKernel adv(
        "upwind_cfl_safe",
        0.9,
        0.92,
        0.80,
        0.88,
        ctx
    );

    MarlArchitecture marl(
        0.90, // policy_alignment_index
        0.86, // rogue_pattern_resilience
        0.84, // multi_actor_scalability
        0.93, // consent_corridor_strength
        0.95, // cybercore_binding_strength
        ctx
    );

    StreamingPipeline stream(
        150.0, // latency ms
        0.88,  // failure_recovery_index
        0.94,  // data_sovereignty_index
        0.30,  // energy_cost_per_event
        0.89,  // biosignal_integration_index
        ctx
    );

    PhoenixThresholds thresholds = PhoenixThresholds::default_thresholds();
    PhoenixStack stack(adv, marl, stream, thresholds);

    // Build matrix rows.
    std::vector<JsonComponentRow> matrix_rows;
    SevenDimProfile adv_p = adv.evaluate_component();
    SevenDimProfile marl_p = marl.evaluate_component();
    SevenDimProfile stream_p = stream.evaluate_component();
    matrix_rows.push_back(JsonComponentRow{adv.id(), adv_p});
    matrix_rows.push_back(JsonComponentRow{marl.id(), marl_p});
    matrix_rows.push_back(JsonComponentRow{stream.id(), stream_p});

    // ASCII matrix to stdout.
    print_ascii_matrix(matrix_rows);

    // System eligibility.
    std::vector<const ComponentEvaluable*> comps;
    comps.push_back(&adv);
    comps.push_back(&marl);
    comps.push_back(&stream);
    SystemEligibility elig = stack.evaluate_system(comps);

    std::cout << "\n=== Phoenix Integrated Eligibility (C++) ===\n";
    std::cout << "Eligible: " << (elig.eligible ? "true" : "false") << "\n";
    std::cout << "Notes:\n" << elig.notes << "\n";

    std::cout << "\nIntegrated profile:\n";
    for (const auto& row : profile_to_rows(elig.profile)) {
        std::cout << "  " << std::setw(22) << dimension_name(row.first)
                  << " = " << std::fixed << std::setprecision(3) << row.second << "\n";
    }

    // JSON export.
    std::string json_matrix = json_component_matrix(matrix_rows);
    std::string json_elig = json_eligibility(elig);
    write_file("ppx_eval_report_matrix.json", json_matrix);
    write_file("ppx_eval_report_eligibility.json", json_elig);

    // ALN export.
    std::string aln = aln_report(matrix_rows, elig);
    write_file("ppx_eval_report.aln", aln);

    std::cout << "\nArtifacts written:\n";
    std::cout << "  ppx_eval_report_matrix.json\n";
    std::cout << "  ppx_eval_report_eligibility.json\n";
    std::cout << "  ppx_eval_report.aln\n";

    return 0;
}
