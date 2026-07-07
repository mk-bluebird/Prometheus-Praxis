//! Time-bounded wrapper for frames.
//!
//! `FrameTimeout` wraps any `Frame<I, O>` and enforces a maximum
//! evaluation time measured in wall-clock milliseconds. If the
//! underlying frame exceeds this budget, `FrameTimeout` returns
//! `None` and does not propagate any partial output.
//!
//! This module is intended for server-side use where `std::time`
//! and `std::thread` are available. It is purely diagnostic and
//! non-actuating: it never touches device drivers or OS-specific
//! syscalls beyond standard threads and timers.

#![forbid(unsafe_code)]

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use crate::frame::Frame;

/// Wrapper that enforces a hard timeout on frame evaluation.
///
/// This is implemented using a worker thread. It is designed for
/// low-frequency diagnostic workloads, not for high-frequency,
/// latency-critical event loops.
#[derive(Clone)]
pub struct FrameTimeout<F> {
    inner: Arc<F>,
    timeout_ms: u64,
}

impl<F> FrameTimeout<F> {
    /// Create a new `FrameTimeout` wrapper for `inner`, with a budget
    /// of `timeout_ms` milliseconds per evaluation.
    pub fn new(inner: F, timeout_ms: u64) -> Self {
        Self {
            inner: Arc::new(inner),
            timeout_ms,
        }
    }
}

impl<I, O, F> Frame<I, O> for FrameTimeout<F>
where
    I: Send + 'static,
    O: Send + 'static,
    F: Frame<I, O> + Send + Sync + 'static,
{
    fn evaluate(&self, input: I) -> Option<O> {
        let inner = Arc::clone(&self.inner);
        let result_slot: Arc<Mutex<Option<Option<O>>>> = Arc::new(Mutex::new(None));
        let result_slot_thread = Arc::clone(&result_slot);

        let handle = thread::spawn(move || {
            let start = Instant::now();
            let output = inner.evaluate(input);
            let elapsed = start.elapsed();

            let mut guard = result_slot_thread
                .lock()
                .expect("FrameTimeout: poisoned mutex");
            *guard = Some(output);

            // Optional: log elapsed via metrics if the `metrics` feature is enabled.
            let _ = elapsed;
        });

        let timeout = Duration::from_millis(self.timeout_ms);
        let join_result = handle.join_timeout(timeout);

        match join_result {
            JoinTimeoutResult::Completed => {
                // Worker finished; extract its result.
                let mut guard = result_slot
                    .lock()
                    .expect("FrameTimeout: poisoned mutex");
                guard.take().unwrap_or(None)
            }
            JoinTimeoutResult::TimedOut => {
                // Worker exceeded timeout. We intentionally leak or detach
                // the worker thread rather than attempting to kill it,
                // because Rust does not support forcible thread cancellation.
                None
            }
            JoinTimeoutResult::Panicked => None,
        }
    }
}

/// Join handle with timeout semantics.
///
/// Rust's standard library does not provide `join_timeout`, so we
/// implement a small helper using `std::thread` and a shared flag.
trait JoinTimeout {
    fn join_timeout(self, timeout: Duration) -> JoinTimeoutResult;
}

enum JoinTimeoutResult {
    Completed,
    TimedOut,
    Panicked,
}

impl<T> JoinTimeout for thread::JoinHandle<T> {
    fn join_timeout(self, timeout: Duration) -> JoinTimeoutResult {
        let (tx, rx) = std::sync::mpsc::channel::<()>();

        let join_handle = thread::spawn(move || {
            let result = self.join();
            let _ = tx.send(());
            result.is_ok()
        });

        if rx.recv_timeout(timeout).is_ok() {
            // Worker finished within timeout.
            let ok = join_handle
                .join()
                .unwrap_or(false);
            if ok {
                JoinTimeoutResult::Completed
            } else {
                JoinTimeoutResult::Panicked
            }
        } else {
            // Timed out. The worker thread may still be running; we do not
            // attempt to cancel it.
            JoinTimeoutResult::TimedOut
        }
    }
}
