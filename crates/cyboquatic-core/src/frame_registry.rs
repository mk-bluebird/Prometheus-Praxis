// filename: crates/cyboquatic-core/src/frame_registry.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use serde::Deserialize;
use std::collections::HashSet;
use std::fs;
use std::path::Path;

/// Frames that can be enabled in this crate.
/// These are purely diagnostic / recognition frames; they do not actuate.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum FrameKind {
    Biodiversity,
    Lyapunov,
    Espd,
}

/// Serializable view of Frames.toml.
///
/// Example Frames.toml:
/// ```toml
/// [frames]
/// biodiversity = true
/// lyapunov = true
/// espd = false
/// ```
#[derive(Debug, Clone, Deserialize)]
struct FramesToml {
    #[serde(default)]
    frames: FramesSection,
}

#[derive(Debug, Clone, Deserialize)]
struct FramesSection {
    #[serde(default)]
    biodiversity: bool,
    #[serde(default)]
    lyapunov: bool,
    #[serde(default)]
    espd: bool,
}

impl Default for FramesSection {
    fn default() -> Self {
        FramesSection {
            biodiversity: false,
            lyapunov: false,
            espd: false,
        }
    }
}

/// Registry of enabled frames, populated from a Frames.toml file.
///
/// This type is designed to be cheap to clone and pass into diagnostic suites.
#[derive(Debug, Clone)]
pub struct FrameRegistry {
    enabled: HashSet<FrameKind>,
}

impl FrameRegistry {
    /// Create an empty registry (no frames enabled).
    pub fn empty() -> Self {
        FrameRegistry {
            enabled: HashSet::new(),
        }
    }

    /// Load frames from a Frames.toml file at the given path.
    ///
    /// If the file is missing or invalid, an error is returned and the caller
    /// can decide whether to fall back to a default registry.
    pub fn from_toml_path<P: AsRef<Path>>(path: P) -> Result<Self, String> {
        let data = fs::read_to_string(&path).map_err(|e| {
            format!(
                "failed to read Frames.toml at {}: {e}",
                path.as_ref().display()
            )
        })?;
        let cfg: FramesToml =
            toml::from_str(&data).map_err(|e| format!("failed to parse Frames.toml: {e}"))?;

        let mut enabled = HashSet::new();
        if cfg.frames.biodiversity {
            enabled.insert(FrameKind::Biodiversity);
        }
        if cfg.frames.lyapunov {
            enabled.insert(FrameKind::Lyapunov);
        }
        if cfg.frames.espd {
            enabled.insert(FrameKind::Espd);
        }

        Ok(FrameRegistry { enabled })
    }

    /// Check if a frame is enabled.
    pub fn is_enabled(&self, kind: FrameKind) -> bool {
        self.enabled.contains(&kind)
    }

    /// Return all enabled frames.
    pub fn enabled_frames(&self) -> impl Iterator<Item = FrameKind> + '_ {
        self.enabled.iter().copied()
    }
}
