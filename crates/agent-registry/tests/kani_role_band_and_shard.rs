#![allow(dead_code)]
#![forbid(unsafe_code)]

use agent_registry::{AgentManifest, RoleBand, ShardBinding};

/// Harness 1: role_band -> trust_band mapping is total and matches ALN semantics.
#[kani::proof]
fn kani_role_band_trust_band_total() {
    let bands = [
        RoleBand::DataIngestor,
        RoleBand::Validator,
        RoleBand::Coordinator,
        RoleBand::Guardian,
    ];

    for band in bands {
        let tb = band.trust_band();
        // Only allowed trust bands from prometheus-role-bands.v1.aln.
        assert!(
            tb == "low" || tb == "medium" || tb == "high" || tb == "sovereign",
            "Unexpected trust band: {}",
            tb
        );
    }
}

/// Harness 2: shard bindings respect prometheus-shard-layout.v1.aln.
///
/// We constrain shards to the known set and assert that is_allowed()
/// matches the ALN invariants for all role-bands.
#[kani::proof]
fn kani_shard_binding_invariants() {
    let bands = [
        RoleBand::DataIngestor,
        RoleBand::Validator,
        RoleBand::Coordinator,
        RoleBand::Guardian,
    ];
    let shard_ids = ["Shard-1", "Shard-2", "Shard-3", "Shard-Guard"];

    for band in bands {
        for shard in shard_ids {
            let binding = ShardBinding {
                agent_id: "agent".to_string(),
                role_band: band,
                shard_id: shard.to_string(),
            };

            let allowed = binding.is_allowed();

            // Encode ALN rules:
            // - DataIngestor -> Shard-1 only
            // - Validator    -> Shard-1 or Shard-2
            // - Coordinator  -> Shard-2 or Shard-3
            // - Guardian     -> Shard-Guard only
            let expected = match (band, shard) {
                (RoleBand::DataIngestor, "Shard-1") => true,
                (RoleBand::Validator, "Shard-1") | (RoleBand::Validator, "Shard-2") => true,
                (RoleBand::Coordinator, "Shard-2") | (RoleBand::Coordinator, "Shard-3") => true,
                (RoleBand::Guardian, "Shard-Guard") => true,
                _ => false,
            };

            assert_eq!(
                allowed, expected,
                "ShardBinding::is_allowed mismatch for {:?} -> {}",
                band, shard
            );
        }
    }
}

/// Harness 3: AgentManifest::new rejects disallowed shard bindings.
#[kani::proof]
fn kani_agent_manifest_rejects_invalid_shard() {
    // DataIngestor on Shard-2 should be rejected.
    let manifest = AgentManifest::new(
        "agent-di".to_string(),
        "DataIngestor",
        "Shard-2".to_string(),
        "agent-di.v1.aln".to_string(),
    );
    assert!(manifest.is_none());

    // Guardian on Shard-Guard should be accepted.
    let manifest_ok = AgentManifest::new(
        "agent-guardian".to_string(),
        "Guardian",
        "Shard-Guard".to_string(),
        "agent-guardian.v1.aln".to_string(),
    );
    assert!(manifest_ok.is_some());
}
