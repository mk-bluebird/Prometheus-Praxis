// filename: crates/steward_identity/examples/energy_mass_roundtrip.rs

use steward_identity::{EnergyMassWindow, StewardIdentity};

const BOSTROM_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
const STEWARD_ID: &str = "wallet_fetch18sd2uj";
const STEWARD_UUID: &str = "87cb8e02-c918-4b2a-aa40-36a8efa37e52";

fn main() {
    let steward = StewardIdentity::new(
        BOSTROM_DID,
        STEWARD_ID,
        STEWARD_UUID,
        "STEWARD",
        "RESEARCH",
    )
    .expect("valid steward identity");

    steward
        .assert_bostrom_prefix("bostrom18sd2uj")
        .expect("prefix OK");

    let window = EnergyMassWindow::new(
        "Node-Gila-001",
        "Phoenix-AZ",
        "water",
        "PFBS",
        "2026-07-16T00:00:00Z",
        "2026-07-16T06:00:00Z",
        3.9e-9,
        3.9e-10,
        0.5,
        1200.0,
        2.592e7,
        1.0,
        2.592e7,
        0.93,
        0.91,
        0.13,
        0.45,
        true,
        true,
        "0xa1b2c3d4e5f67890",
        steward,
    );

    let json = serde_json::to_string_pretty(&window).expect("serialize");
    println!("{json}");
}
