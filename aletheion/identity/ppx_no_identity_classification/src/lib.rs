// Path: aletheion/identity/ppx_no_identity_classification/src/lib.rs

use serde::{Deserialize, Serialize};
use std::{
    collections::{HashMap, HashSet, VecDeque},
    fs,
    path::Path,
};

/// ALN-derived config types ---------------------------------------------------

#[derive(Debug, Clone, Deserialize)]
pub struct ContinuityCrate {
    pub crate_name: String,
    pub version_semver: String,
    pub git_ref: String,
    pub enabled: bool,
}

#[derive(Debug, Clone, Deserialize)]
pub struct ForbiddenSymbol {
    pub symbol_fq: String,
    pub symbol_kind: String, // TYPE, FUNCTION, TRAIT, FIELD, MODULE
}

#[derive(Debug, Clone, Deserialize)]
pub struct ForbiddenSet {
    pub label: String,
    pub symbols: Vec<ForbiddenSymbol>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct SourceSinkPolicy {
    pub label: String,
    pub source_type_names: Vec<String>,
    pub sink_type_names: Vec<String>,
    pub allow_paths: bool,
}

#[derive(Debug, Clone, Deserialize)]
pub struct InvariantBinding {
    pub crate_name: String,
    pub forbidden_label: String,
    pub policy_label: String,
    pub required: bool,
}

#[derive(Debug, Clone, Deserialize)]
pub struct PpxConfig {
    pub continuity_crates: Vec<ContinuityCrate>,
    pub forbidden_sets: Vec<ForbiddenSet>,
    pub policies: Vec<SourceSinkPolicy>,
    pub bindings: Vec<InvariantBinding>,
}

/// Simple symbol + call-graph model -------------------------------------------

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct SymbolId(pub String);

#[derive(Debug, Clone)]
pub struct CallGraph {
    /// Map from symbol -> symbols it calls
    pub edges: HashMap<SymbolId, HashSet<SymbolId>>,
}

impl CallGraph {
    pub fn new() -> Self {
        Self { edges: HashMap::new() }
    }

    pub fn add_edge(&mut self, from: impl Into<String>, to: impl Into<String>) {
        let from_id = SymbolId(from.into());
        let to_id = SymbolId(to.into());
        self.edges.entry(from_id).or_default().insert(to_id);
    }

    /// Breadth-first search: is there a path from any source to any sink?
    pub fn any_path(&self, sources: &HashSet<SymbolId>, sinks: &HashSet<SymbolId>) -> bool {
        let mut visited: HashSet<SymbolId> = HashSet::new();
        let mut queue: VecDeque<SymbolId> = sources.iter().cloned().collect();
        while let Some(sym) = queue.pop_front() {
            if sinks.contains(&sym) {
                return true;
            }
            if !visited.insert(sym.clone()) {
                continue;
            }
            if let Some(nexts) = self.edges.get(&sym) {
                for n in nexts {
                    if !visited.contains(n) {
                        queue.push_back(n.clone());
                    }
                }
            }
        }
        false
    }
}

/// External symbol inventory & graph loader -----------------------------------
/// In your stack this should be populated from rustc metadata / `cargo metadata`
/// or a dedicated analysis pass run in CI.[file:9]

#[derive(Debug, Clone)]
pub struct CrateSymbols {
    pub crate_name: String,
    pub defined_symbols: HashSet<String>,
    pub call_graph: CallGraph,
}

pub trait CrateSymbolProvider {
    fn load_symbols(&self, crate_name: &str) -> anyhow::Result<CrateSymbols>;
}

/// Validation error type ------------------------------------------------------

#[derive(Debug, thiserror::Error)]
pub enum PpxInvariantError {
    #[error("Failed to read PPX config JSON: {0}")]
    Io(#[from] std::io::Error),

    #[error("Failed to parse PPX config JSON: {0}")]
    Parse(#[from] serde_json::Error),

    #[error("No binding for crate `{0}`")]
    MissingBinding(String),

    #[error("Forbidden symbol `{symbol}` found in crate `{crate_name}`")]
    ForbiddenSymbol {
        crate_name: String,
        symbol: String,
    },

    #[error("Source-sink paths exist in crate `{crate_name}` for policy `{policy}`")]
    SourceSinkPath {
        crate_name: String,
        policy: String,
    },
}

/// Core validator -------------------------------------------------------------

pub struct PpxNoIdentityClassificationValidator<P: CrateSymbolProvider> {
    cfg: PpxConfig,
    provider: P,
}

impl<P: CrateSymbolProvider> PpxNoIdentityClassificationValidator<P> {
    pub fn load_from_json_path(path: impl AsRef<Path>, provider: P) -> Result<Self, PpxInvariantError> {
        let raw = fs::read_to_string(path)?;
        let cfg: PpxConfig = serde_json::from_str(&raw)?;
        Ok(Self { cfg, provider })
    }

    fn forbidden_set_by_label(&self, label: &str) -> Option<&ForbiddenSet> {
        self.cfg.forbidden_sets.iter().find(|s| s.label == label)
    }

    fn policy_by_label(&self, label: &str) -> Option<&SourceSinkPolicy> {
        self.cfg.policies.iter().find(|p| p.label == label)
    }

    fn binding_for_crate(&self, crate_name: &str) -> Option<&InvariantBinding> {
        self.cfg.bindings.iter().find(|b| b.crate_name == crate_name)
    }

    /// Run all PPX invariants for all enabled continuity crates.
    pub fn validate_all(&self) -> Result<(), PpxInvariantError> {
        for cc in &self.cfg.continuity_crates {
            if !cc.enabled {
                continue;
            }
            self.validate_crate(&cc.crate_name)?;
        }
        Ok(())
    }

    /// Validate a single crate by name.
    pub fn validate_crate(&self, crate_name: &str) -> Result<(), PpxInvariantError> {
        let binding = self
            .binding_for_crate(crate_name)
            .ok_or_else(|| PpxInvariantError::MissingBinding(crate_name.to_string()))?;

        if !binding.required {
            return Ok(());
        }

        let fset = self
            .forbidden_set_by_label(&binding.forbidden_label)
            .expect("ALN config must define forbidden set");

        let policy = self
            .policy_by_label(&binding.policy_label)
            .expect("ALN config must define source-sink policy");

        let symbols = self.provider.load_symbols(crate_name)?;

        self.check_forbidden_symbols(crate_name, fset, &symbols)?;
        self.check_source_sink_paths(crate_name, policy, &symbols)?;

        Ok(())
    }

    fn check_forbidden_symbols(
        &self,
        crate_name: &str,
        forbidden: &ForbiddenSet,
        symbols: &CrateSymbols,
    ) -> Result<(), PpxInvariantError> {
        for sym in &forbidden.symbols {
            if symbols.defined_symbols.contains(&sym.symbol_fq) {
                return Err(PpxInvariantError::ForbiddenSymbol {
                    crate_name: crate_name.to_string(),
                    symbol: sym.symbol_fq.clone(),
                });
            }
        }
        Ok(())
    }

    fn check_source_sink_paths(
        &self,
        crate_name: &str,
        policy: &SourceSinkPolicy,
        symbols: &CrateSymbols,
    ) -> Result<(), PpxInvariantError> {
        if policy.allow_paths {
            // Nothing to enforce if paths are allowed.
            return Ok(());
        }

        let mut sources: HashSet<SymbolId> = HashSet::new();
        let mut sinks: HashSet<SymbolId> = HashSet::new();

        for s in &policy.source_type_names {
            if symbols.defined_symbols.contains(s) {
                sources.insert(SymbolId(s.clone()));
            }
        }
        for s in &policy.sink_type_names {
            if symbols.defined_symbols.contains(s) {
                sinks.insert(SymbolId(s.clone()));
            }
        }

        if sources.is_empty() || sinks.is_empty() {
            // If crate doesn't define either, there is no path to check.
            return Ok(());
        }

        if symbols.call_graph.any_path(&sources, &sinks) {
            return Err(PpxInvariantError::SourceSinkPath {
                crate_name: crate_name.to_string(),
                policy: policy.label.clone(),
            });
        }

        Ok(())
    }
}

/// Convenience entrypoint for build.rs / CI -----------------------------------

pub fn validate_ppx_invariants_or_panic<P: CrateSymbolProvider>(
    cfg_path: impl AsRef<Path>,
    provider: P,
) {
    let validator = PpxNoIdentityClassificationValidator::load_from_json_path(cfg_path, provider)
        .expect("Failed to load PPX config");
    if let Err(e) = validator.validate_all() {
        eprintln!("PPX-NO-IDENTITY-CLASSIFICATION violation: {e}");
        panic!("PPX-NO-IDENTITY-CLASSIFICATION invariant failed");
    }
}
