// filepath: crates/psychrisk_engine/src/types.rs
#![forbid(unsafe_code)]

#[derive(Clone, Debug)]
pub struct MentalIntegrityDimensions {
    pub cognitive_integrity: f32,
    pub affective_integrity: f32,
    pub narrative_integrity: f32,
    pub social_integrity: f32,
}

#[derive(Clone, Debug)]
pub struct MentalIntegrityPolicy {
    pub continuity_required: bool,

    pub min_cognitive_integrity_for_perkunos: f32,
    pub min_affective_integrity_for_perkunos: f32,
    pub min_narrative_integrity_for_perkunos: f32,
    pub min_social_integrity_for_perkunos: f32,

    pub min_pci_for_amber_uplift_short: f32,
    pub min_pci_for_amber_uplift_long: f32,

    pub max_cogload_delta_per_min_floor: f32,
    pub max_cogload_delta_per_min_amber: f32,
}

#[derive(Clone, Debug)]
pub struct MentalIntegrityDoctrine {
    pub invariant_not_weaker_than_adult_floor: bool,
}

#[derive(Clone, Debug)]
pub enum PciWindowKind {
    Short15M,
    Long24H,
}

#[derive(Clone, Debug)]
pub struct PciWindow {
    pub kind: PciWindowKind,
    pub pci_value: f32,
}

#[derive(Clone, Debug)]
pub struct MentalIntegrityBinding {
    pub dimensions: MentalIntegrityDimensions,
    pub policy: MentalIntegrityPolicy,
    pub doctrine: MentalIntegrityDoctrine,

    pub pci_short_15m: PciWindow,
    pub pci_long_24h: PciWindow,
}

#[derive(Clone, Debug)]
pub struct AmberUpliftDecision {
    pub uplift_allowed: bool,
    pub reason: String,
}
