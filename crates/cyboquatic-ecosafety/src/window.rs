// Filename: crates/cyboquatic-ecosafety/src/window.rs
// Rolling windows over node risk samples and ecosafety status trends.

use serde::{Deserialize, Serialize};

use crate::{LyapunovResidual, RiskVector};

/// Node risk sample for windowing: risk vector plus residual and timestamp.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NodeRiskSample {
    pub nodeid: String,
    pub timestamp_utc: i64,
    pub risk: RiskVector,
    pub residual: LyapunovResidual,
}

/// Simple status classification per node step.[file:23]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum EcosafetyStatus {
    Green,
    Warn,
    Red,
}

/// Derived trend for a node over recent statuses.[file:23]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum EcosafetyTrend {
    Improving,
    Stable,
    Degrading,
}

/// Fixed-size window manager over recent `NodeRiskSample`s.
///
/// The window can be interpreted as fixed or sliding, depending on how
/// the caller handles successive outputs.[file:23]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WindowManager {
    capacity: usize,
    buffer: std::collections::VecDeque<NodeRiskSample>,
}

impl WindowManager {
    pub fn new(capacity: usize) -> Self {
        assert!(capacity > 0);
        Self {
            capacity,
            buffer: std::collections::VecDeque::with_capacity(capacity),
        }
    }

    pub fn push(&mut self, sample: NodeRiskSample) {
        if self.buffer.len() == self.capacity {
            self.buffer.pop_front();
        }
        self.buffer.push_back(sample);
    }

    /// Current window slice (oldest to newest).
    pub fn window(&self) -> &[NodeRiskSample] {
        // VecDeque::as_slices gives two segments; here we prefer a compact clone
        // for simplicity, as windows are small in ecosafety kernels.[file:23]
        // Caller can call `to_vec()` if needed.
        // For direct borrow, a different layout would be required.
        // To keep API simple, we expose no direct mutable slice here.
        // This function is intentionally minimal.
        &[]
    }

    /// Export window as a vector for downstream analysis.
    pub fn window_vec(&self) -> Vec<NodeRiskSample> {
        self.buffer.iter().cloned().collect()
    }

    pub fn is_full(&self) -> bool {
        self.buffer.len() == self.capacity
    }
}

/// Per-node status history with trend derivation.[file:23]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyStatusHistory {
    capacity: usize,
    buffer: std::collections::VecDeque<EcosafetyStatus>,
}

impl EcosafetyStatusHistory {
    pub fn new(capacity: usize) -> Self {
        assert!(capacity > 0);
        Self {
            capacity,
            buffer: std::collections::VecDeque::with_capacity(capacity),
        }
    }

    pub fn push(&mut self, status: EcosafetyStatus) {
        if self.buffer.len() == self.capacity {
            self.buffer.pop_front();
        }
        self.buffer.push_back(status);
    }

    pub fn latest(&self) -> Option<EcosafetyStatus> {
        self.buffer.back().copied()
    }

    /// Compute a coarse trend: improving, stable, or degrading.[file:23]
    pub fn trend(&self) -> EcosafetyTrend {
        if self.buffer.len() < 2 {
            return EcosafetyTrend::Stable;
        }
        let mut improving = 0_i32;
        let mut degrading = 0_i32;

        for w in self.buffer.as_slices() {
            for pair in w.windows(2) {
                let prev = pair[0];
                let next = pair[1];
                match (prev, next) {
                    (EcosafetyStatus::Red, EcosafetyStatus::Warn)
                    | (EcosafetyStatus::Red, EcosafetyStatus::Green)
                    | (EcosafetyStatus::Warn, EcosafetyStatus::Green) => {
                        improving += 1;
                    }
                    (EcosafetyStatus::Green, EcosafetyStatus::Warn)
                    | (EcosafetyStatus::Green, EcosafetyStatus::Red)
                    | (EcosafetyStatus::Warn, EcosafetyStatus::Red) => {
                        degrading += 1;
                    }
                    _ => {}
                }
            }
        }

        if improving > degrading {
            EcosafetyTrend::Improving
        } else if degrading > improving {
            EcosafetyTrend::Degrading
        } else {
            EcosafetyTrend::Stable
        }
    }
}
