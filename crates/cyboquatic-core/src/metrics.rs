// filename: crates/cyboquatic-core/src/metrics.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::Scalar;
use std::sync::OnceLock;
use std::time::Duration;

#[cfg(feature = "metrics")]
use std::sync::Mutex;

/// Prometheus-style metric snapshot for a single evaluation pass.
/// This is intentionally simple and text-based; an external endpoint
/// (outside this crate) is expected to scrape or forward these lines.
#[derive(Debug, Clone)]
pub struct MetricsSnapshot {
    pub eval_duration_ms: f64,
    pub condition_number: Option<Scalar>,
    pub ecosafety_distance: Option<Scalar>,
    pub k_score: Option<Scalar>,
    pub e_score: Option<Scalar>,
    pub r_score: Option<Scalar>,
}

impl MetricsSnapshot {
    pub fn to_prometheus_lines(&self, prefix: &str) -> String {
        let mut out = String::new();
        out.push_str(&format!(
            "# cyboquatic-core metrics snapshot (prefix={prefix})\n"
        ));
        out.push_str(&format!(
            "{prefix}_eval_duration_ms {}\n",
            self.eval_duration_ms
        ));
        if let Some(cn) = self.condition_number {
            out.push_str(&format!("{prefix}_condition_number {}\n", cn));
        }
        if let Some(d) = self.ecosafety_distance {
            out.push_str(&format!("{prefix}_ecosafety_distance {}\n", d));
        }
        if let Some(k) = self.k_score {
            out.push_str(&format!("{prefix}_k_score {}\n", k));
        }
        if let Some(e) = self.e_score {
            out.push_str(&format!("{prefix}_e_score {}\n", e));
        }
        if let Some(r) = self.r_score {
            out.push_str(&format!("{prefix}_r_score {}\n", r));
        }
        out
    }
}

fn duration_to_ms(d: Duration) -> f64 {
    (d.as_secs_f64()) * 1000.0
}

#[cfg(feature = "metrics")]
static LAST_SNAPSHOT: OnceLock<Mutex<Option<MetricsSnapshot>>> = OnceLock::new();

#[cfg(feature = "metrics")]
fn last_snapshot_cell() -> &'static Mutex<Option<MetricsSnapshot>> {
    LAST_SNAPSHOT.get_or_init(|| Mutex::new(None))
}

/// Record a new metrics snapshot (no-op if `metrics` feature is disabled).
pub fn record_metrics_snapshot(
    eval_duration: Duration,
    condition_number: Option<Scalar>,
    ecosafety_distance: Option<Scalar>,
    k: Option<Scalar>,
    e: Option<Scalar>,
    r: Option<Scalar>,
) {
    #[cfg(feature = "metrics")]
    {
        let snap = MetricsSnapshot {
            eval_duration_ms: duration_to_ms(eval_duration),
            condition_number,
            ecosafety_distance,
            k_score: k,
            e_score: e,
            r_score: r,
        };
        if let Ok(mut guard) = last_snapshot_cell().lock() {
            *guard = Some(snap);
        }
    }

    #[cfg(not(feature = "metrics"))]
    {
        let _ = eval_duration;
        let _ = condition_number;
        let _ = ecosafety_distance;
        let _ = k;
        let _ = e;
        let _ = r;
    }
}

/// Export the last recorded metrics snapshot as Prometheus-style lines.
/// Returns `None` if no snapshot is available or metrics are disabled.
pub fn export_last_metrics(prefix: &str) -> Option<String> {
    #[cfg(feature = "metrics")]
    {
        if let Ok(guard) = last_snapshot_cell().lock() {
            if let Some(snap) = guard.as_ref() {
                return Some(snap.to_prometheus_lines(prefix));
            }
        }
        None
    }

    #[cfg(not(feature = "metrics"))]
    {
        let _ = prefix;
        None
    }
}
