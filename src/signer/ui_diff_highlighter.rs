// filename: src/signer/ui_diff_highlighter.rs
// destination: eco_restoration_shard/src/signer/ui_diff_highlighter.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use crate::governance::types::ResearchManifest;
use crate::safety::errors::SabotageError;
use crate::bci::BciInterface;

/// A single field-level change detected between two manifests.
#[derive(Clone, Debug)]
pub struct ManifestFieldChange {
    pub field_name: String,
    pub old_value: Option<String>,
    pub new_value: Option<String>,
}

/// Result of a diff highlighting session.
#[derive(Clone, Debug)]
pub struct DiffReviewResult {
    pub acknowledged_changes: Vec<ManifestFieldChange>,
}

/// Computes a field-wise diff between two manifests.
pub fn compute_manifest_diff(
    previous: &ResearchManifest,
    current: &ResearchManifest,
) -> Vec<ManifestFieldChange> {
    let mut changes = Vec::new();

    if previous.name != current.name {
        changes.push(ManifestFieldChange {
            field_name: "name".to_string(),
            old_value: Some(previous.name.clone()),
            new_value: Some(current.name.clone()),
        });
    }

    if previous.description != current.description {
        changes.push(ManifestFieldChange {
            field_name: "description".to_string(),
            old_value: Some(previous.description.clone()),
            new_value: Some(current.description.clone()),
        });
    }

    if previous.license != current.license {
        changes.push(ManifestFieldChange {
            field_name: "license".to_string(),
            old_value: Some(previous.license.clone()),
            new_value: Some(current.license.clone()),
        });
    }

    if previous.dependencies != current.dependencies {
        changes.push(ManifestFieldChange {
            field_name: "dependencies".to_string(),
            old_value: Some(previous.dependencies.join(", ")),
            new_value: Some(current.dependencies.join(", ")),
        });
    }

    changes
}

/// Interactive diff highlighter: requires a BCI acknowledgement per change.
pub fn run_diff_highlighter_ui<B: BciInterface>(
    bci: &B,
    previous: &ResearchManifest,
    current: &ResearchManifest,
) -> Result<DiffReviewResult, SabotageError> {
    let changes = compute_manifest_diff(previous, current);

    if changes.is_empty() {
        return Ok(DiffReviewResult {
            acknowledged_changes: Vec::new(),
        });
    }

    let mut acknowledged = Vec::new();

    for change in &changes {
        render_change_highlight(change);

        let cue = format!(
            "Approve change in field '{}' from '{:?}' to '{:?}'",
            change.field_name, change.old_value, change.new_value
        );

        let approved = bci.request_neural_ack(&cue);
        if !approved {
            return Err(SabotageError::UnacknowledgedChange {
                field_name: change.field_name.clone(),
            });
        }

        acknowledged.push(change.clone());
    }

    Ok(DiffReviewResult {
        acknowledged_changes: acknowledged,
    })
}

/// Simple terminal rendering; GUI frontends can override this.
fn render_change_highlight(change: &ManifestFieldChange) {
    println!("======== CHANGE DETECTED ========");
    println!("Field: {}", change.field_name);
    println!("Old:   {:?}", change.old_value);
    println!("New:   {:?}", change.new_value);
    println!("(Highlighted in red in GUI)");
}
