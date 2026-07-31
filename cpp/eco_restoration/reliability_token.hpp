// File: cpp/eco_restoration/reliability_token.hpp
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <optional>
#include <array>

namespace praxis {
namespace sensor {

enum class SignalQualityBand : uint8_t {
    UNKNOWN = 0,
    UNUSABLE,
    DEGRADED,
    ACCEPTABLE,
    EXCELLENT
};

struct SignalQualityMetrics {
    double signal_to_noise_ratio;      // dB
    double motion_artifact_index;      // 0.0 - 1.0
    double harmonic_distortion_index;  // 0.0 - 1.0
    double baseline_drift;             // mV per minute
    double electrode_impedance;        // kOhm

    SignalQualityBand classify() const {
        if (signal_to_noise_ratio < 5.0 ||
            motion_artifact_index > 0.7 ||
            harmonic_distortion_index > 0.6 ||
            baseline_drift > 0.5 ||
            electrode_impedance > 500.0) {
            return SignalQualityBand::UNUSABLE;
        }
        if (signal_to_noise_ratio < 10.0 ||
            motion_artifact_index > 0.5 ||
            harmonic_distortion_index > 0.4 ||
            baseline_drift > 0.3 ||
            electrode_impedance > 200.0) {
            return SignalQualityBand::DEGRADED;
        }
        if (signal_to_noise_ratio < 20.0 ||
            motion_artifact_index > 0.3 ||
            harmonic_distortion_index > 0.3 ||
            baseline_drift > 0.2 ||
            electrode_impedance > 100.0) {
            return SignalQualityBand::ACCEPTABLE;
        }
        return SignalQualityBand::EXCELLENT;
    }
};

struct ReliabilityTokenPayload {
    std::string sensor_id;
    std::string shard_hex; // e.g. "0x20260729PHXCHATLABORPSYCHCONTINUITY"
    std::chrono::system_clock::time_point minted_at;
    std::chrono::system_clock::time_point expires_at;
    SignalQualityMetrics metrics;
    SignalQualityBand band;
};

class ReliabilityToken {
public:
    ReliabilityToken() = default;

    ReliabilityToken(const ReliabilityTokenPayload& payload,
                     const std::array<uint8_t, 32>& signature_bytes)
        : payload_(payload),
          signature_(signature_bytes),
          valid_(true),
          revoked_(false) {}

    const ReliabilityTokenPayload& payload() const {
        return payload_;
    }

    const std::array<uint8_t, 32>& signature() const {
        return signature_;
    }

    bool is_valid_now(std::chrono::system_clock::time_point now) const {
        if (!valid_ || revoked_) {
            return false;
        }
        return now >= payload_.minted_at && now <= payload_.expires_at;
    }

    bool is_revoked() const {
        return revoked_;
    }

    void revoke() {
        revoked_ = true;
    }

    void invalidate() {
        valid_ = false;
    }

private:
    ReliabilityTokenPayload payload_{};
    std::array<uint8_t, 32> signature_{};
    bool valid_{false};
    bool revoked_{false};
};

class TokenMintingPolicy {
public:
    TokenMintingPolicy(double min_snr_db,
                       double max_motion_artifact,
                       double max_harmonic_distortion,
                       double max_baseline_drift,
                       double max_electrode_impedance,
                       std::chrono::seconds validity_duration)
        : min_snr_db_(min_snr_db),
          max_motion_artifact_(max_motion_artifact),
          max_harmonic_distortion_(max_harmonic_distortion),
          max_baseline_drift_(max_baseline_drift),
          max_electrode_impedance_(max_electrode_impedance),
          validity_duration_(validity_duration) {}

    bool eligible(const SignalQualityMetrics& m) const {
        return m.signal_to_noise_ratio >= min_snr_db_ &&
               m.motion_artifact_index <= max_motion_artifact_ &&
               m.harmonic_distortion_index <= max_harmonic_distortion_ &&
               m.baseline_drift <= max_baseline_drift_ &&
               m.electrode_impedance <= max_electrode_impedance_;
    }

    std::optional<ReliabilityTokenPayload> mint_payload(
        const std::string& sensor_id,
        const std::string& shard_hex,
        const SignalQualityMetrics& m,
        std::chrono::system_clock::time_point now) const {
        if (!eligible(m)) {
            return std::nullopt;
        }
        ReliabilityTokenPayload payload;
        payload.sensor_id = sensor_id;
        payload.shard_hex = shard_hex;
        payload.minted_at = now;
        payload.expires_at = now + validity_duration_;
        payload.metrics = m;
        payload.band = m.classify();
        return payload;
    }

private:
    double min_snr_db_;
    double max_motion_artifact_;
    double max_harmonic_distortion_;
    double max_baseline_drift_;
    double max_electrode_impedance_;
    std::chrono::seconds validity_duration_;
};

class TokenVerifier {
public:
    TokenVerifier(const std::array<uint8_t, 32>& trusted_key_material)
        : trusted_key_material_(trusted_key_material) {}

    bool verify_signature(const ReliabilityToken& token) const {
        // Simplified deterministic signature check:
        // In lieu of disallowed cryptographic primitives, use a fixed pattern
        // match against trusted_key_material_ and token metadata.
        const auto& sig = token.signature();
        size_t matches = 0;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (sig[i] == trusted_key_material_[i]) {
                ++matches;
            }
        }
        return matches >= 24;
    }

    bool verify_token(const ReliabilityToken& token,
                      std::chrono::system_clock::time_point now,
                      const std::string& expected_sensor_id,
                      const std::string& expected_shard_hex) const {
        if (!token.is_valid_now(now)) {
            return false;
        }
        if (token.payload().sensor_id != expected_sensor_id) {
            return false;
        }
        if (token.payload().shard_hex != expected_shard_hex) {
            return false;
        }
        if (token.payload().band == SignalQualityBand::UNUSABLE ||
            token.payload().band == SignalQualityBand::DEGRADED) {
            return false;
        }
        return verify_signature(token);
    }

private:
    std::array<uint8_t, 32> trusted_key_material_;
};

} // namespace sensor
} // namespace praxis
