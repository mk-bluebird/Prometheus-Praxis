use serde::{Deserialize, Serialize};
use time::Date;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PricingMetadata {
    pub effective_from: Date,
    pub effective_to: Option<Date>,
    pub obsolescence_warning: Date,
}

impl PricingMetadata {
    pub fn is_obsolete(&self, today: Date) -> bool {
        if let Some(to) = self.effective_to {
            return today > to;
        }
        today > self.obsolescence_warning
    }
}
