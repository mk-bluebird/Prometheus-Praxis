// Path: aletheion/erm/funding/src/qf_domain_params.rs

use rust_decimal::Decimal;
use rust_decimal_macros::dec;

#[derive(Clone, Copy, Debug)]
pub enum QfDomain {
    Health,
    EcoRestoration,
    NeurorightsTooling,
}

#[derive(Clone, Copy, Debug)]
pub struct DomainAlphaConfig {
    pub alpha: Decimal,
    pub max_credits_for_qf: Decimal,
}

pub fn alpha_for_domain(domain: QfDomain) -> DomainAlphaConfig {
    match domain {
        QfDomain::Health => DomainAlphaConfig {
            alpha: dec!(0.03),              // slower attenuation
            max_credits_for_qf: dec!(5000), // health labor often needs scale
        },
        QfDomain::EcoRestoration => DomainAlphaConfig {
            alpha: dec!(0.07),              // stronger anti-whale bias
            max_credits_for_qf: dec!(2000),
        },
        QfDomain::NeurorightsTooling => DomainAlphaConfig {
            alpha: dec!(0.10),              // most plutocracy-resistant
            max_credits_for_qf: dec!(1000),
        },
    }
}
