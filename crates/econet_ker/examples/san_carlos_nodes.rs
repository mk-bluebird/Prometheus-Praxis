// File: crates/econet_ker/examples/san_carlos_nodes.rs

use econet_ker::cell::EcoCell;
use econet_ker::env::EnvClass;
use econet_ker::module::EcoModule;
use econet_ker::policy::KerPolicy;
use econet_ker::roles::CyboRole;

fn main() {
    let policy = KerPolicy::default_v0();

    let tb_cell = EcoCell::new(
        "EcoNet_AQ_TB_v0",
        "neutral tofu-brine aqueous battery",
        vec![EnvClass::F, EnvClass::C, EnvClass::U, EnvClass::D],
        0.55,
        0.85,
        0.08,
        "EOL_TB_v0",
        vec![CyboRole::Sensing, CyboRole::Cleanup, CyboRole::Aeration, CyboRole::ShoreHub],
        &policy,
    );

    let sentinel_module = EcoModule::new(
        "SanCarlos_Sentinel_v0",
        EnvClass::F,
        CyboRole::Sensing,
        vec![tb_cell],
        true,
    );

    let valid = sentinel_module.validate(&policy);
    println!("San Carlos sentinel module valid: {valid}");
}
