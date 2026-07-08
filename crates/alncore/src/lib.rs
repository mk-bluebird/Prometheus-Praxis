// filename: crates/alncore/src/lib.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

pub mod model;
pub mod parser;
pub mod eval;

pub use crate::model::{
    AlnDocument,
    DeployDecisionKernel,
    KerCompleteness,
    KerSnapshot,
    Lane,
    RepoManifest,
    SafeStepRule,
};

pub use crate::parser::{parse_aln_str, AlnParseError};
pub use crate::eval::{eval_safestep, eval_deploy};
