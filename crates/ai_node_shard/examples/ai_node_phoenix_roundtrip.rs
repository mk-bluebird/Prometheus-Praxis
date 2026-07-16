// filename: crates/ai_node_shard/examples/ai_node_phoenix_roundtrip.rs

use ai_node_shard::{
    init_phoenix_hex_schema, insert_phoenix_hex_anchor, AINodeShard, PhoenixHexAnchor,
};
use rusqlite::Connection;
use steward_identity::StewardIdentity;

const BOSTROM_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
const STEWARD_ID: &str = "wallet_fetch18sd2uj";
const STEWARD_UUID: &str = "87cb8e02-c918-4b2a-aa40-36a8efa37e52";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. Steward identity
    let steward = StewardIdentity::new(
        BOSTROM_DID,
        STEWARD_ID,
        STEWARD_UUID,
        "STEWARD",
        "RESEARCH",
    )?;
    steward.assert_bostrom_prefix("bostrom18sd2uj")?;

    // 2. AI node shard (single measurement window)
    let shard = AINodeShard {
        nodeid: "AI-PHX-001".into(),
        region: "Phoenix-AZ".into(),
        lane: "RESEARCH".into(),
        steward,
        twindow_start: "2026-07-16T00:00:00Z".into(),
        twindow_end: "2026-07-16T01:00:00Z".into(),
        core_energy_kwh_per_workload: 0.25,
        joules_per_inference: 3.5,
        pue: 1.15,
        cue_kg_co2_per_kwh: 0.20,
        eco_per_joule: 0.75,
        throughput_tokens_per_s: 1200.0,
        throughput_inferences_per_s: 35.0,
        utilization_pct: 76.0,
        ere: 0.10,
        eco_task_ratio_pct: 65.0,
        wue_l_per_kwh: 0.9,
        embodied_kg_co2eq: 15.0,
        k: 0.93,
        e: 0.91,
        r: 0.13,
        vt: 0.42,
        strength_index_s: 0.88,
        evidencehex: "0xa3f5c7e9b1d20468c7e4a9d2b5f81357".into(),
        signinghex: BOSTROM_DID.into(),
    };

    shard.validate()?;
    let json = serde_json::to_string_pretty(&shard)?;
    println!("AINodeShard JSON:\n{json}");

    // 3. Phoenix Hex registry entry for this shard
    let anchor = PhoenixHexAnchor::new(
        shard.evidencehex.clone(),
        "SHARD",
        "PHX_AI_ENERGY_DV_20260716",
        "qpudatashards/particles/AINodePhoenix2026v1.csv",
        STEWARD_UUID.to_string(),
        None,
    )?;

    let conn = Connection::open("phoenix_hex_registry.sqlite3")?;
    init_phoenix_hex_schema(&conn)?;
    insert_phoenix_hex_anchor(&conn, &anchor)?;

    println!("Registered anchor {}", anchor.hex_id);

    Ok(())
}
