// filename: eco_wealth/src/lib.rs
// destination: eco_restoration_shard/eco_wealth/src/lib.rs

pub mod model;
pub mod tseries;
pub mod spine;
pub mod shards;
pub mod nonactuating;

// Public re-exports
pub use crate::model::{
    EcoWealthAmount, EcoWealthUnit, PortfolioId, StewardId, EcoWealthSnapshot,
};
pub use crate::tseries::{EcoWealthTimePoint, EcoWealthSeries};
pub use crate::spine::{SpineConnection, PortfolioQuery};
pub use crate::shards::{InvestmentProposal, RestorationBond};
pub use crate::nonactuating::{EcoWealthKernel, compute_eco_wealth_snapshot};
