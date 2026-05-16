// filename: src/signer/deny_list.rs
// destination: eco_restoration_shard/src/signer/deny_list.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use crate::governance::types::ResearchManifest;
use crate::rpc::bostrom_client::BostromClient;
use crate::safety::errors::SabotageError;

/// Compact deny-list representation, using hashes and Bloom filters without blake.
pub struct DenyListState {
    pub merkle_root: Vec<u8>,
    pub bloom_filter: Vec<u8>,
}

pub struct DenyListChecker<C: BostromClient> {
    pub client: C,
}

impl<C: BostromClient> DenyListChecker<C> {
    pub fn check_deny_list(
        &self,
        manifest: &ResearchManifest,
    ) -> Result<(), SabotageError> {
        let state = self.load_deny_list_state()?;
        let strings = collect_manifest_strings(manifest);

        for s in strings {
            if self.bloom_might_contain(&state.bloom_filter, &s) {
                return Err(SabotageError::ForbiddenPattern {
                    value: s,
                });
            }
        }

        Ok(())
    }

    fn load_deny_list_state(&self) -> Result<DenyListState, SabotageError> {
        let merkle_root = self.client.query_deny_list_root()?;
        let bloom_filter = self.client.query_deny_list_bloom()?;
        Ok(DenyListState { merkle_root, bloom_filter })
    }

    fn bloom_might_contain(&self, bloom: &[u8], value: &str) -> bool {
        let hash = simple_hash(value.as_bytes());
        let index = (hash % (bloom.len() as u64)) as usize;
        bloom.get(index).map(|b| *b != 0).unwrap_or(false)
    }
}

fn collect_manifest_strings(manifest: &ResearchManifest) -> Vec<String> {
    let mut result = Vec::new();
    result.push(manifest.name.clone());
    result.push(manifest.description.clone());
    result.push(manifest.license.clone());
    for dep in &manifest.dependencies {
        result.push(dep.clone());
    }
    result
}

fn simple_hash(bytes: &[u8]) -> u64 {
    let mut acc: u64 = 1469598103934665603;
    for b in bytes {
        acc ^= *b as u64;
        acc = acc.wrapping_mul(1099511628211);
    }
    acc
}
