// filename: src/governance/manifest_template.rs
// destination: eco_restoration_shard/src/governance/manifest_template.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use crate::governance::types::{ResearchManifest, DailyEvolutionError};
use crate::identity::{BciPublicKey, BciSignature};
use crate::crypto::hash::ManifestHash;
use crate::storage::templates::HostTemplateStore;

/// Trait for validating a ResearchManifest against a host-approved template.
pub trait ManifestTemplateValidator {
    fn validate_against_template(
        &self,
        manifest: &ResearchManifest,
        host_bci_key: &BciPublicKey,
    ) -> Result<(), DailyEvolutionError>;
}

/// Concrete validator using a serialized, BCI-signed template.
pub struct SignedTemplateValidator<'a> {
    pub store: &'a dyn HostTemplateStore,
}

impl<'a> ManifestTemplateValidator for SignedTemplateValidator<'a> {
    fn validate_against_template(
        &self,
        manifest: &ResearchManifest,
        host_bci_key: &BciPublicKey,
    ) -> Result<(), DailyEvolutionError> {
        let manifest_logical = manifest.template_logicalname();
        let template = self
            .store
            .load_template(&manifest_logical)
            .ok_or(DailyEvolutionError::MissingTemplate {
                logicalname: manifest_logical.clone(),
            })?;

        if !verify_template_signature(&template, host_bci_key) {
            return Err(DailyEvolutionError::InvalidTemplateSignature {
                logicalname: manifest_logical,
            });
        }

        if template.expected_license != manifest.license {
            return Err(DailyEvolutionError::SabotageDetected {
                reason: "license field mismatch from host-approved template".to_string(),
            });
        }

        if template.expected_dependencies != manifest.dependencies {
            return Err(DailyEvolutionError::SabotageDetected {
                reason: "dependencies mismatch from host-approved template".to_string(),
            });
        }

        Ok(())
    }
}

pub struct SignedTemplate {
    pub logicalname: String,
    pub expected_license: String,
    pub expected_dependencies: Vec<String>,
    pub template_hash: ManifestHash,
    pub bci_signature: BciSignature,
}

fn verify_template_signature(template: &SignedTemplate, host_bci_key: &BciPublicKey) -> bool {
    let mut data = Vec::new();
    data.extend_from_slice(template.logicalname.as_bytes());
    data.extend_from_slice(template.expected_license.as_bytes());
    for dep in &template.expected_dependencies {
        data.extend_from_slice(dep.as_bytes());
    }
    data.extend_from_slice(template.template_hash.as_bytes());

    host_bci_key.verify(&data, &template.bci_signature)
}

/// Example run_daily_evolution wiring.
pub fn run_daily_evolution<V: ManifestTemplateValidator>(
    manifest: &ResearchManifest,
    validator: &V,
    host_bci_key: &BciPublicKey,
) -> Result<(), DailyEvolutionError> {
    validator.validate_against_template(manifest, host_bci_key)?;
    // existing evolution logic follows using the validated manifest
    Ok(())
}
