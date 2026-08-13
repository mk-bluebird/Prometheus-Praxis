#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cybercore::cyboquatics {

inline constexpr const char* kHostDid = "didalnorganic-host";
inline constexpr const char* kBostromAddress =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
inline constexpr const char* kMigrationClause =
    "ALN.MIGRATION.CYBERCORE_AUTHORITY.v1";

enum class FacilityKind : std::uint8_t {
    School,
    Hospital,
    Other
};

enum class Decision : std::uint8_t {
    Allow,
    Derate,
    Stop
};

enum class ViolationCode : std::uint8_t {
    None,
    InvalidPolicy,
    AuthorityMismatch,
    MissingActionCommitment,
    MissingPolicyCommitment,
    IncompleteFacilityInventory,
    StaleForecast,
    InvalidForecast,
    ProtectedFacilityHeatExceeded,
    RiskOfHarmExceeded,
    BiocompatibilityInsufficient,
    PainIndexExceeded,
    FearIndexOutsideBand,
    LyapunovDerateRequired,
    NoSafeDerateAvailable
};

struct RiskWeights final {
    double physical{0.34};
    double cyber{0.21};
    double psychological{0.30};
    double uncertainty{0.15};
};

struct RiskBreakdown final {
    double thermal{0.0};
    double kinetic{0.0};
    double chemical{0.0};
    double cyber_integrity{0.0};
    double cyber_latency{0.0};
    double cognitive_load{0.0};
    double emotional_destabilization{0.0};
    double identity_drift{0.0};
    double physiological_impact{0.0};
    double uncertainty{0.0};
};

struct BiophysicalEnvelope final {
    double biocompatibility{1.0};
    double pain_index{0.0};
    double fear_index{0.50};
};

struct FacilityHeatForecast final {
    FacilityKind facility_kind{FacilityKind::Other};
    double distance_m{0.0};
    double predicted_heat_index{0.0};
    double uncertainty_bound{0.0};
    std::uint64_t observation_epoch_s{0};
    bool geometry_verified{false};
};

struct MacroActionContext final {
    std::string host_did;
    std::string bostrom_address;
    std::string migration_clause;
    std::string action_commitment;
    std::string policy_commitment;

    bool facility_inventory_complete{false};
    bool ker_within_lane{false};
    bool roh_within_ceiling{false};
    bool tsafe_within_band{false};
    bool lyapunov_non_increasing{false};
    bool safe_derate_exists{false};

    RiskBreakdown risk;
    BiophysicalEnvelope biophysical;
    std::vector<FacilityHeatForecast> facility_forecasts;
};

struct VulnerableImpactEnvelope final {
    std::string policy_commitment;
    std::uint64_t policy_epoch{0};

    double evaluation_radius_m{500.0};
    double default_critical_heat_index{0.0};
    double minimum_heat_margin{0.0};
    double max_forecast_age_s{0.0};

    double roh_ceiling{0.30};
    double minimum_biocompatibility{0.57};
    double maximum_pain_index{0.73};
    double minimum_fear_index{0.31};
    double maximum_fear_index{0.68};

    RiskWeights risk_weights{};
};

struct AuditEntry final {
    Decision decision{Decision::Stop};
    ViolationCode violation{ViolationCode::InvalidPolicy};
    double computed_roh{1.0};
    double minimum_heat_slack{0.0};
    std::string action_commitment;
    std::string policy_commitment;
    std::string reason;
};

class ActuationAuthorization final {
public:
    ActuationAuthorization(const ActuationAuthorization&) = default;
    ActuationAuthorization(ActuationAuthorization&&) noexcept = default;
    ActuationAuthorization& operator=(const ActuationAuthorization&) = default;
    ActuationAuthorization& operator=(ActuationAuthorization&&) noexcept = default;

    const std::string& action_commitment() const noexcept;
    const std::string& policy_commitment() const noexcept;
    std::uint64_t policy_epoch() const noexcept;
    std::uint64_t expires_at_epoch_s() const noexcept;
    bool valid_for(
        const std::string& action_commitment,
        const std::string& policy_commitment,
        std::uint64_t now_epoch_s) const noexcept;

private:
    friend class VulnerableImpactGate;

    ActuationAuthorization(
        std::string action_commitment,
        std::string policy_commitment,
        std::uint64_t policy_epoch,
        std::uint64_t expires_at_epoch_s);

    std::string action_commitment_;
    std::string policy_commitment_;
    std::uint64_t policy_epoch_{0};
    std::uint64_t expires_at_epoch_s_{0};
};

class VulnerableImpactGate final {
public:
    explicit VulnerableImpactGate(VulnerableImpactEnvelope envelope);

    const VulnerableImpactEnvelope& envelope() const noexcept;

    bool policy_valid() const noexcept;

    double compute_roh(const RiskBreakdown& risk) const noexcept;

    AuditEntry evaluate(
        const MacroActionContext& context,
        std::uint64_t now_epoch_s) const;

    bool authorize(
        const MacroActionContext& context,
        std::uint64_t now_epoch_s,
        std::uint64_t authorization_ttl_s,
        ActuationAuthorization& authorization,
        AuditEntry& audit) const;

private:
    bool authority_matches(const MacroActionContext& context) const noexcept;
    bool biophysical_within_envelope(
        const BiophysicalEnvelope& envelope) const noexcept;
    bool valid_forecast(const FacilityHeatForecast& forecast) const noexcept;
    bool protected_facility(FacilityKind kind) const noexcept;
    double critical_heat_index(const FacilityHeatForecast& forecast) const noexcept;

    VulnerableImpactEnvelope envelope_;
};

}  // namespace cybercore::cyboquatics
