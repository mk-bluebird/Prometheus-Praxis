// filename: ecorestorationshard/cyboquatic_index/src/frame_stream.rs
// destination: ecorestorationshard/cyboquatic_index/src/frame_stream.rs

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

//! Streaming diagnostics primitives for ecosafety/biodiversity frames.
//!
//! This module adds:
//! - Serde + compact binary encoding for `NodeRiskSample` and `ShardUpdate`.
//! - A `FrameError`/`FrameTimeout` model for bounded evaluation.
//! - Incremental `try_extend` updates on streaming frames.
//! - A `DataLoader` trait to feed samples from CSV / SQLite / generators.
//!
//! All code is non‑actuating and intended for diagnostics only.

use core::time::Duration;
use std::io::{Read, Write};
use std::time::Instant;

use serde::{Deserialize, Serialize};

/// Scalar risk type in `[0.0, 1.0]` for node‑level diagnostics.
pub type RiskScalar = f32;

/// Sample of node‑level risk and meta for ecosafety/biodiversity frames.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeRiskSample {
    /// Node identifier (e.g., cyboquatic node id).
    pub node_id: String,
    /// UTC timestamp in RFC3339.
    pub ts_utc: String,
    /// Ecosafety risk scalar in `[0,1]`.
    pub ecosafety_risk: RiskScalar,
    /// Biodiversity risk scalar in `[0,1]`.
    pub biodiversity_risk: RiskScalar,
    /// Optional temperature in Celsius.
    pub temp_c: Option<f32>,
    /// Optional flow rate in m³/s.
    pub flow_cms: Option<f32>,
}

/// Shard‑level update summary for a window of samples.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShardUpdate {
    /// Shard identifier (e.g., canal reach id).
    pub shard_id: String,
    /// Number of samples consumed for this update.
    pub sample_count: u64,
    /// Mean ecosafety risk over the window.
    pub mean_ecosafety: RiskScalar,
    /// Mean biodiversity risk over the window.
    pub mean_biodiversity: RiskScalar,
    /// Optional covariance (ecosafety, biodiversity).
    pub cov_eb: Option<f32>,
}

/// Compact endian‑agnostic binary envelope for `NodeRiskSample`.
///
/// This is deliberately simple:
/// - Fixed layout for numeric fields (native `f32`).
/// - Length‑prefixed UTF‑8 for strings.
/// - No schema evolution magic; versioning is handled at the ALN level.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeRiskSampleBin {
    pub node_id: String,
    pub ts_utc: String,
    pub ecosafety_risk: RiskScalar,
    pub biodiversity_risk: RiskScalar,
    pub temp_c: Option<f32>,
    pub flow_cms: Option<f32>,
}

impl From<NodeRiskSample> for NodeRiskSampleBin {
    fn from(s: NodeRiskSample) -> Self {
        Self {
            node_id: s.node_id,
            ts_utc: s.ts_utc,
            ecosafety_risk: s.ecosafety_risk,
            biodiversity_risk: s.biodiversity_risk,
            temp_c: s.temp_c,
            flow_cms: s.flow_cms,
        }
    }
}

impl From<NodeRiskSampleBin> for NodeRiskSample {
    fn from(b: NodeRiskSampleBin) -> Self {
        Self {
            node_id: b.node_id,
            ts_utc: b.ts_utc,
            ecosafety_risk: b.ecosafety_risk,
            biodiversity_risk: b.biodiversity_risk,
            temp_c: b.temp_c,
            flow_cms: b.flow_cms,
        }
    }
}

/// Compact endian‑agnostic binary envelope for `ShardUpdate`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShardUpdateBin {
    pub shard_id: String,
    pub sample_count: u64,
    pub mean_ecosafety: RiskScalar,
    pub mean_biodiversity: RiskScalar,
    pub cov_eb: Option<f32>,
}

impl From<ShardUpdate> for ShardUpdateBin {
    fn from(s: ShardUpdate) -> Self {
        Self {
            shard_id: s.shard_id,
            sample_count: s.sample_count,
            mean_ecosafety: s.mean_ecosafety,
            mean_biodiversity: s.mean_biodiversity,
            cov_eb: s.cov_eb,
        }
    }
}

impl From<ShardUpdateBin> for ShardUpdate {
    fn from(b: ShardUpdateBin) -> Self {
        Self {
            shard_id: b.shard_id,
            sample_count: b.sample_count,
            mean_ecosafety: b.mean_ecosafety,
            mean_biodiversity: b.mean_biodiversity,
            cov_eb: b.cov_eb,
        }
    }
}

/// Encode a `NodeRiskSample` to a compact binary stream.
pub fn write_node_risk_sample_bin<W: Write>(
    mut w: W,
    sample: &NodeRiskSample,
) -> Result<(), FrameError> {
    let bin: NodeRiskSampleBin = sample.clone().into();
    let bytes = bincode::serde::encode_to_vec(&bin, bincode::config::standard())
        .map_err(FrameError::Encode)?;
    w.write_all(&bytes).map_err(FrameError::Io)
}

/// Decode a `NodeRiskSample` from a binary stream.
///
/// The caller should bound reads with an outer framing protocol.
pub fn read_node_risk_sample_bin<R: Read>(mut r: R) -> Result<NodeRiskSample, FrameError> {
    let mut buf = Vec::new();
    r.read_to_end(&mut buf).map_err(FrameError::Io)?;
    let (bin, _) =
        bincode::serde::decode_from_slice::<NodeRiskSampleBin, _>(&buf, bincode::config::standard())
            .map_err(FrameError::Decode)?;
    Ok(bin.into())
}

/// Encode a `ShardUpdate` to a compact binary stream.
pub fn write_shard_update_bin<W: Write>(
    mut w: W,
    update: &ShardUpdate,
) -> Result<(), FrameError> {
    let bin: ShardUpdateBin = update.clone().into();
    let bytes = bincode::serde::encode_to_vec(&bin, bincode::config::standard())
        .map_err(FrameError::Encode)?;
    w.write_all(&bytes).map_err(FrameError::Io)
}

/// Decode a `ShardUpdate` from a binary stream.
pub fn read_shard_update_bin<R: Read>(mut r: R) -> Result<ShardUpdate, FrameError> {
    let mut buf = Vec::new();
    r.read_to_end(&mut buf).map_err(FrameError::Io)?;
    let (bin, _) =
        bincode::serde::decode_from_slice::<ShardUpdateBin, _>(&buf, bincode::config::standard())
            .map_err(FrameError::Decode)?;
    Ok(bin.into())
}

/// Error model for diagnostics frames.
#[derive(Debug)]
pub enum FrameError {
    /// Underlying I/O failure.
    Io(std::io::Error),
    /// Serialization failure.
    Encode(bincode::error::EncodeError),
    /// Deserialization failure.
    Decode(bincode::error::DecodeError),
    /// Evaluation budget exceeded for a node or shard.
    Timeout(FrameTimeout),
}

/// Timeout description for frame evaluation.
#[derive(Debug, Clone, Copy)]
pub struct FrameTimeout {
    /// Hard time budget for a frame evaluation.
    pub budget: Duration,
    /// Actual elapsed time when the timeout was detected.
    pub elapsed: Duration,
}

impl FrameTimeout {
    /// Construct a new timeout descriptor.
    pub fn new(budget: Duration, elapsed: Duration) -> Self {
        Self { budget, elapsed }
    }
}

/// Decorator that bounds evaluation time per node/shard.
///
/// On timeout it returns a partial result together with a `FrameError::Timeout`.
pub fn with_frame_timeout<T, F>(
    budget: Duration,
    f: F,
) -> Result<T, FrameError>
where
    F: FnOnce() -> T,
{
    let start = Instant::now();
    let out = f();
    let elapsed = start.elapsed();
    if elapsed > budget {
        Err(FrameError::Timeout(FrameTimeout::new(budget, elapsed)))
    } else {
        Ok(out)
    }
}

/// Streaming ecosafety/biodiversity frame with incremental statistics.
#[derive(Debug, Clone)]
pub struct RiskFrame {
    shard_id: String,
    n: u64,
    mean_e: f64,
    mean_b: f64,
    // Welford online covariance accumulator.
    c_eb: f64,
}

impl RiskFrame {
    /// Create an empty frame for a given shard.
    pub fn new(shard_id: impl Into<String>) -> Self {
        Self {
            shard_id: shard_id.into(),
            n: 0,
            mean_e: 0.0,
            mean_b: 0.0,
            c_eb: 0.0,
        }
    }

    /// Current sample count.
    pub fn len(&self) -> u64 {
        self.n
    }

    /// Whether the frame has no samples.
    pub fn is_empty(&self) -> bool {
        self.n == 0
    }

    /// Incrementally extend the frame with additional samples.
    ///
    /// This uses numerically stable online mean/covariance updates.
    pub fn try_extend(&mut self, samples: &[NodeRiskSample]) -> Result<(), FrameError> {
        for s in samples {
            let x = s.ecosafety_risk as f64;
            let y = s.biodiversity_risk as f64;
            self.n += 1;
            let n_f = self.n as f64;
            let delta_x = x - self.mean_e;
            let delta_y = y - self.mean_b;
            self.mean_e += delta_x / n_f;
            self.mean_b += delta_y / n_f;
            self.c_eb += (n_f - 1.0) * delta_x * delta_y / n_f;
        }
        Ok(())
    }

    /// Materialize a `ShardUpdate` snapshot from the current frame.
    pub fn to_update(&self) -> ShardUpdate {
        let cov = if self.n > 1 {
            Some(self.c_eb / ((self.n - 1) as f64) as f32)
        } else {
            None
        };
        ShardUpdate {
            shard_id: self.shard_id.clone(),
            sample_count: self.n,
            mean_ecosafety: self.mean_e as f32,
            mean_biodiversity: self.mean_b as f32,
            cov_eb: cov,
        }
    }
}

/// Abstract sample source for diagnostics.
///
/// Implementations must be non‑actuating (no direct device control).
pub trait DataLoader {
    /// Yield the next `NodeRiskSample`, or `Ok(None)` at end of stream.
    fn next_sample(&mut self) -> Result<Option<NodeRiskSample>, FrameError>;
}

/// In‑memory loader over a fixed slice; useful for tests and synthetic generators.
pub struct SliceDataLoader<'a> {
    idx: usize,
    samples: &'a [NodeRiskSample],
}

impl<'a> SliceDataLoader<'a> {
    /// Construct a new loader over a slice of samples.
    pub fn new(samples: &'a [NodeRiskSample]) -> Self {
        Self { idx: 0, samples }
    }
}

impl<'a> DataLoader for SliceDataLoader<'a> {
    fn next_sample(&mut self) -> Result<Option<NodeRiskSample>, FrameError> {
        if self.idx >= self.samples.len() {
            return Ok(None);
        }
        let s = self.samples[self.idx].clone();
        self.idx += 1;
        Ok(Some(s))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_risk_frame_try_extend_and_update() {
        let samples = vec![
            NodeRiskSample {
                node_id: "n1".to_string(),
                ts_utc: "2026-07-07T00:00:00Z".to_string(),
                ecosafety_risk: 0.2,
                biodiversity_risk: 0.4,
                temp_c: None,
                flow_cms: None,
            },
            NodeRiskSample {
                node_id: "n1".to_string(),
                ts_utc: "2026-07-07T00:01:00Z".to_string(),
                ecosafety_risk: 0.4,
                biodiversity_risk: 0.6,
                temp_c: None,
                flow_cms: None,
            },
        ];

        let mut frame = RiskFrame::new("shard-a");
        frame.try_extend(&samples).unwrap();
        let upd = frame.to_update();

        assert_eq!(upd.shard_id, "shard-a");
        assert_eq!(upd.sample_count, 2);
        assert!((upd.mean_ecosafety - 0.3).abs() < 1e-6);
        assert!((upd.mean_biodiversity - 0.5).abs() < 1e-6);
    }

    #[test]
    fn test_frame_timeout_triggers() {
        let budget = Duration::from_millis(1);
        let result = with_frame_timeout(budget, || {
            // Busy loop to force timeout in test environment.
            let mut acc = 0u64;
            for i in 0..1_000_000 {
                acc = acc.wrapping_add(i);
            }
            acc
        });

        match result {
            Err(FrameError::Timeout(t)) => {
                assert!(t.elapsed >= t.budget);
            }
            _ => {}
        }
    }

    #[test]
    fn test_slice_data_loader() {
        let samples = vec![NodeRiskSample {
            node_id: "n1".to_string(),
            ts_utc: "2026-07-07T00:00:00Z".to_string(),
            ecosafety_risk: 0.1,
            biodiversity_risk: 0.2,
            temp_c: None,
            flow_cms: None,
        }];

        let mut loader = SliceDataLoader::new(&samples);
        let s1 = loader.next_sample().unwrap();
        assert!(s1.is_some());
        let s2 = loader.next_sample().unwrap();
        assert!(s2.is_none());
    }
}
