//! Sovereign Phoenix CityPass core crate
//! Design (D): Low risk (local-only, explicit invariants)
//! Neuro-Risk (NR): Very low (no biophysical inputs or actuation)
//! Energy-Efficiency (EE): High (offline verification, CPU-only)

pub mod model;
pub mod binding;
pub mod revocation;
pub mod verify;
pub mod eco;
pub mod hexcommit;

pub use crate::model::{CityCapability, CityCapabilityDomain};
pub use crate::binding::{CityPassBinding, BindingId, bind_city_pass};
pub use crate::revocation::{RevocationRecord, RevocationStore, RevocationReason};
pub use crate::verify::{VerificationError, VerificationOutcome, verify_tap};
pub use crate::eco::{EcoImpact, EcoConfig};
pub use crate::hexcommit::HexCommit;
