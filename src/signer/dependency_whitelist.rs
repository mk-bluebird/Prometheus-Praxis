// filename: src/signer/dependency_whitelist.rs
// destination: eco_restoration_shard/src/signer/dependency_whitelist.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use crate::governance::types::ResearchManifest;
use crate::safety::errors::SabotageError;
use crate::rpc::bostrom_client::BostromClient;

/// Manifest dependency tuple.
#[derive(Clone, Debug)]
pub struct DependencyTuple {
    pub crate_name: String,
    pub version: String,
    pub hash: String,
}

/// Verifies that each dependency is in the host's whitelist Merkle tree.
pub fn verify_dependencies_against_whitelist<C: BostromClient>(
    client: &C,
    manifest: &ResearchManifest,
) -> Result<(), SabotageError> {
    let whitelist = client.query_dependency_whitelist()?;

    for dep in &manifest.dependencies {
        let tuple = resolve_dependency_tuple(dep)?;
        if !client.verify_merkle_inclusion(
            &whitelist.merkle_root,
            &tuple_hash(&tuple),
            &tuple.proof,
        ) {
            return Err(SabotageError::DependencyNotWhitelisted {
                crate_name: tuple.crate_name,
                version: tuple.version,
            });
        }
    }

    Ok(())
}

fn resolve_dependency_tuple(dep: &str) -> Result<DependencyTuple, SabotageError> {
    let parts: Vec<&str> = dep.split('@').collect();
    if parts.len() != 2 {
        return Err(SabotageError::InvalidDependencyFormat {
            value: dep.to_string(),
        });
    }
    Ok(DependencyTuple {
        crate_name: parts[0].to_string(),
        version: parts[1].to_string(),
        hash: String::new(),
    })
}

fn tuple_hash(tuple: &DependencyTuple) -> Vec<u8> {
    let mut data = Vec::new();
    data.extend_from_slice(tuple.crate_name.as_bytes());
    data.extend_from_slice(tuple.version.as_bytes());
    data
}
