// Sacrifice Zone Envelope for Prometheus-Praxis / eco_restoration_shard.
//
// Role:
//   - Rust mirror of specs/aln/SacrificeZoneSpec.v1.aln.
//   - Provides pure, non-actuating predicates for KER/RoH and eligibility checks.
//   - Designed for integration into governance guards, nanoswarm planners,
//     and observability crates, without any IO or hardware control.
//
// Requirements:
//   - Rust edition 2024, rust-version = "1.85".
//   - forbid(unsafe_code).
//   - No networking, file IO, or hardware calls in this crate.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Exclusion status for a sacrifice zone.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ExclusionStatus {
    None,
    HumanExclusion,
    IndustrialExclusion,
    FullExclusion,
}

/// Primary contamination class.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum PrimaryClass {
    Radiation,
    HeavyMetals,
    PersistentOrganics,
    Hydrocarbons,
    Microplastics,
    AirPollution,
    MultiModal,
}

/// Biosphere activity level (pollinators / wildlife).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ActivityLevel {
    NoneDetected,
    Sparse,
    Moderate,
    High,
}

/// Prometheus-Praxis lane enum (aligned with existing lanes).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum Lane {
    Research,
    Pilot,
    Production,
    CityCritical,
}

/// Simple KER snapshot (0..1 normalized).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
pub struct KerSnapshot {
    pub k: f32,
    pub e: f32,
    pub r: f32,
}

/// Simple RoH snapshot (0..1 normalized).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
pub struct RohSnapshot {
    pub roh: f32,
}

/// Sacrifice Zone Envelope (Rust mirror of a row from SacrificeZoneSpec.v1.aln).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SacrificeZoneEnvelope {
    pub zone_id: String,
    pub region_id: String,
    pub authority_id: String,
    pub geometry_ref: String,
    pub exclusion_status: ExclusionStatus,
    pub evidence_bundle_id: String,
    pub proof_hash_hex: String,
    pub primary_class: PrimaryClass,
    pub secondary_classes_json: String,
    pub contamination_radiation: f32,
    pub contamination_heavy_metals: f32,
    pub contamination_organics: f32,
    pub contamination_microplastics: f32,
    pub contamination_air: f32,
    pub contamination_other: f32,
    pub pollinator_activity: ActivityLevel,
    pub wildlife_activity: ActivityLevel,
    pub biosignal_proof_id: String,
    pub lane: Lane,
    pub roh_ceiling: f32,
    pub kmin: f32,
    pub emin: f32,
    pub rmax: f32,
    pub neurorights_envelope_id: String,
    pub sovereignty_tags_json: String,
    pub notes: String,
}

impl SacrificeZoneEnvelope {
    /// Global RoH ceiling; non-offsettable, aligned with Praxis kernels.
    const ROH_GLOBAL_MAX: f32 = 0.30;

    /// Construct a new envelope, enforcing structural invariants at the boundary.
    ///
    /// This function is intended to be used by ALN loaders and governance
    /// shells; any violation indicates a configuration or migration error.
    pub fn new(
        zone_id: String,
        region_id: String,
        authority_id: String,
        geometry_ref: String,
        exclusion_status: ExclusionStatus,
        evidence_bundle_id: String,
        proof_hash_hex: String,
        primary_class: PrimaryClass,
        secondary_classes_json: String,
        contamination_radiation: f32,
        contamination_heavy_metals: f32,
        contamination_organics: f32,
        contamination_microplastics: f32,
        contamination_air: f32,
        contamination_other: f32,
        pollinator_activity: ActivityLevel,
        wildlife_activity: ActivityLevel,
        biosignal_proof_id: String,
        lane: Lane,
        roh_ceiling: f32,
        kmin: f32,
        emin: f32,
        rmax: f32,
        neurorights_envelope_id: String,
        sovereignty_tags_json: String,
        notes: String,
    ) -> Result<Self, String> {
        // Basic required fields.
        if zone_id.is_empty() {
            return Err("zone_id must not be empty".into());
        }
        if geometry_ref.is_empty() {
            return Err("geometry_ref must not be empty".into());
        }
        if proof_hash_hex.is_empty() {
            return Err("proof_hash_hex must not be empty".into());
        }

        // RoH ceiling invariants: roh_ceiling <= ROH_GLOBAL_MAX.
        if roh_ceiling < 0.0 || roh_ceiling > Self::ROH_GLOBAL_MAX {
            return Err(format!(
                "roh_ceiling {} must be in [0.0, {}]",
                roh_ceiling, Self::ROH_GLOBAL_MAX
            ));
        }

        // KER targets: normalized and consistent with RoH.
        if kmin < 0.0 || kmin > 1.0 {
            return Err(format!("kmin {} must be in [0.0, 1.0]", kmin));
        }
        if emin < 0.0 || emin > 1.0 {
            return Err(format!("emin {} must be in [0.0, 1.0]", emin));
        }
        if rmax < 0.0 || rmax > roh_ceiling {
            return Err(format!(
                "rmax {} must be in [0.0, roh_ceiling={}]",
                rmax, roh_ceiling
            ));
        }

        // Contamination indices must be normalized to [0, 1].
        for (name, v) in &[
            ("contamination_radiation", contamination_radiation),
            ("contamination_heavy_metals", contamination_heavy_metals),
            ("contamination_organics", contamination_organics),
            ("contamination_microplastics", contamination_microplastics),
            ("contamination_air", contamination_air),
            ("contamination_other", contamination_other),
        ] {
            if *v < 0.0 || *v > 1.0 {
                return Err(format!("{name} {v} must be in [0.0, 1.0]"));
            }
        }

        Ok(Self {
            zone_id,
            region_id,
            authority_id,
            geometry_ref,
            exclusion_status,
            evidence_bundle_id,
            proof_hash_hex,
            primary_class,
            secondary_classes_json,
            contamination_radiation,
            contamination_heavy_metals,
            contamination_organics,
            contamination_microplastics,
            contamination_air,
            contamination_other,
            pollinator_activity,
            wildlife_activity,
            biosignal_proof_id,
            lane,
            roh_ceiling,
            kmin,
            emin,
            rmax,
            neurorights_envelope_id,
            sovereignty_tags_json,
            notes,
        })
    }

    /// Check whether a given KER snapshot lies within this envelope.
    ///
    /// Conditions:
    ///   - k >= kmin
    ///   - e >= emin
    ///   - r <= rmax
    pub fn ker_within(&self, ker: KerSnapshot) -> bool {
        ker.k >= self.kmin && ker.e >= self.emin && ker.r <= self.rmax
    }

    /// Check whether a given RoH snapshot respects this zone's ceiling.
    ///
    /// Conditions:
    ///   - roh <= roh_ceiling
    ///   - roh <= ROH_GLOBAL_MAX
    pub fn roh_within(&self, roh: RohSnapshot) -> bool {
        roh.roh <= self.roh_ceiling && roh.roh <= Self::ROH_GLOBAL_MAX
    }

    /// Determine whether the zone can be treated as "lifeless" for nanoswarm
    /// targeting purposes (strict, forward-only heuristic).
    ///
    /// Conditions:
    ///   - exclusion_status is at least HumanExclusion.
    ///   - pollinator_activity == NoneDetected.
    ///   - wildlife_activity == NoneDetected.
    ///
    /// This does not authorize actuation; it only classifies zones under a
    /// stricter "no lifeforms detected" assumption for further guard layers.
    pub fn lifeless_classification(&self) -> bool {
        let exclusion_ok = matches!(
            self.exclusion_status,
            ExclusionStatus::HumanExclusion
                | ExclusionStatus::IndustrialExclusion
                | ExclusionStatus::FullExclusion
        );
        let pollinator_none = matches!(self.pollinator_activity, ActivityLevel::NoneDetected);
        let wildlife_none = matches!(self.wildlife_activity, ActivityLevel::NoneDetected);
        exclusion_ok && pollinator_none && wildlife_none
    }

    /// Eligibility predicate for nanoswarm deployment proposals in this zone.
    ///
    /// This is a pure guard, intended to be composed with RoH/KER/Lyapunov
    /// guards and neurorights/public-good guards.
    ///
    /// Conditions:
    ///   - lifeless_classification() == true.
    ///   - lane is Research or Pilot (no direct Production/CityCritical).
    pub fn eligible_for_nanoswarm(&self) -> bool {
        if !self.lifeless_classification() {
            return false;
        }

        matches!(self.lane, Lane::Research | Lane::Pilot)
    }

    /// Monotone safety check between a previous and a new envelope for the
    /// same zone_id. This is intended for migration/upgrade validators.
    ///
    /// Conditions (strictly safer or equal):
    ///   - roh_ceiling_new <= roh_ceiling_old.
    ///   - rmax_new <= rmax_old.
    ///   - KER floors non-decreasing: kmin_new >= kmin_old, emin_new >= emin_old.
    ///   - Contamination indices do not decrease silently (no "washing" without evidence):
    ///       contamination_*_new >= contamination_*_old.
    ///
    /// Sovereignty and neurorights flags are treated as protection bits and
    /// should not be weakened; that enforcement is delegated to higher-level
    /// governance kernels that parse sovereignty_tags_json and
    /// neurorights_envelope_id.
    pub fn monotone_upgrade(&self, prev: &Self) -> bool {
        if self.zone_id != prev.zone_id {
            return false;
        }

        let roh_ok = self.roh_ceiling <= prev.roh_ceiling;
        let rmax_ok = self.rmax <= prev.rmax;
        let kmin_ok = self.kmin >= prev.kmin;
        let emin_ok = self.emin >= prev.emin;

        let contamination_ok = self.contamination_radiation >= prev.contamination_radiation
            && self.contamination_heavy_metals >= prev.contamination_heavy_metals
            && self.contamination_organics >= prev.contamination_organics
            && self.contamination_microplastics >= prev.contamination_microplastics
            && self.contamination_air >= prev.contamination_air
            && self.contamination_other >= prev.contamination_other;

        roh_ok && rmax_ok && kmin_ok && emin_ok && contamination_ok
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn base_envelope() -> SacrificeZoneEnvelope {
        SacrificeZoneEnvelope::new(
            "SZ.TEST.001".to_string(),
            "REGION.TEST".to_string(),
            "did:org:test".to_string(),
            "geo.szone.test.001".to_string(),
            ExclusionStatus::HumanExclusion,
            "evidence-bundle.test.001".to_string(),
            "0xHASH".to_string(),
            PrimaryClass::PersistentOrganics,
            "[]".to_string(),
            0.5,
            0.5,
            0.5,
            0.5,
            0.5,
            0.5,
            ActivityLevel::NoneDetected,
            ActivityLevel::NoneDetected,
            "biosignal.test.001".to_string(),
            Lane::Research,
            0.25,
            0.80,
            0.80,
            0.20,
            "neurorights.envelope.citizen.v1".to_string(),
            "[]".to_string(),
            "Test zone".to_string(),
        )
        .expect("valid base envelope")
    }

    #[test]
    fn test_ker_within() {
        let env = base_envelope();
        let ker_ok = KerSnapshot {
            k: 0.85,
            e: 0.90,
            r: 0.15,
        };
        assert!(env.ker_within(ker_ok));

        let ker_bad_r = KerSnapshot {
            k: 0.85,
            e: 0.90,
            r: 0.25,
        };
        assert!(!env.ker_within(ker_bad_r));
    }

    #[test]
    fn test_roh_within() {
        let env = base_envelope();
        let roh_ok = RohSnapshot { roh: 0.20 };
        assert!(env.roh_within(roh_ok));

        let roh_bad = RohSnapshot { roh: 0.35 };
        assert!(!env.roh_within(roh_bad));
    }

    #[test]
    fn test_lifeless_classification_and_eligibility() {
        let env = base_envelope();
        assert!(env.lifeless_classification());
        assert!(env.eligible_for_nanoswarm());
    }

    #[test]
    fn test_monotone_upgrade() {
        let prev = base_envelope();
        let newer = SacrificeZoneEnvelope::new(
            prev.zone_id.clone(),
            prev.region_id.clone(),
            prev.authority_id.clone(),
            prev.geometry_ref.clone(),
            prev.exclusion_status,
            prev.evidence_bundle_id.clone(),
            prev.proof_hash_hex.clone(),
            prev.primary_class,
            prev.secondary_classes_json.clone(),
            prev.contamination_radiation + 0.01,
            prev.contamination_heavy_metals + 0.01,
            prev.contamination_organics + 0.01,
            prev.contamination_microplastics + 0.01,
            prev.contamination_air + 0.01,
            prev.contamination_other + 0.01,
            prev.pollinator_activity,
            prev.wildlife_activity,
            prev.biosignal_proof_id.clone(),
            prev.lane,
            prev.roh_ceiling - 0.01,
            prev.kmin + 0.01,
            prev.emin + 0.01,
            prev.rmax - 0.01,
            prev.neurorights_envelope_id.clone(),
            prev.sovereignty_tags_json.clone(),
            prev.notes.clone(),
        )
        .expect("valid upgraded envelope");

        assert!(newer.monotone_upgrade(&prev));
    }
}
