// crates/cyboquatic_benchmark_tool/src/lib.rs

#![forbid(unsafe_code)]
#![deny(warnings)]

//! Cyboquatic benchmark harness for ecosafety and governance kernels.

use std::time::{Duration, Instant};

use econet_governance_spine::{
    EcosafetyInput, EcosafetyKernel, EcosafetyOutput, GovernanceKernel, GovernanceOutput,
};

/// Named benchmark for a single kernel invocation pattern.
#[derive(Debug, Clone)]
pub struct BenchmarkCase<I> {
    pub name: String,
    pub iterations: u64,
    pub input: I,
}

impl<I> BenchmarkCase<I> {
    pub fn new<N: Into<String>>(name: N, iterations: u64, input: I) -> Self {
        Self {
            name: name.into(),
            iterations,
            input,
        }
    }
}

/// Result for a single benchmark case.
#[derive(Debug, Clone)]
pub struct BenchmarkResult {
    pub name: String,
    pub iterations: u64,
    pub total: Duration,
    pub per_iter: Duration,
}

impl BenchmarkResult {
    pub fn iter_per_second(&self) -> f64 {
        if self.total.as_secs_f64() == 0.0 {
            return self.iterations as f64;
        }
        self.iterations as f64 / self.total.as_secs_f64()
    }
}

/// Benchmark suite aggregating multiple results.
#[derive(Debug, Clone)]
pub struct BenchmarkSuite {
    pub ecosafety_results: Vec<BenchmarkResult>,
    pub governance_results: Vec<BenchmarkResult>,
}

impl BenchmarkSuite {
    pub fn new(
        ecosafety_results: Vec<BenchmarkResult>,
        governance_results: Vec<BenchmarkResult>,
    ) -> Self {
        Self {
            ecosafety_results,
            governance_results,
        }
    }

    pub fn all_results(&self) -> impl Iterator<Item = &BenchmarkResult> {
        self.ecosafety_results.iter().chain(self.governance_results.iter())
    }
}

/// Run a benchmark for an ecosafety kernel using the provided case.
pub fn run_ecosafety_benchmark<K>(
    kernel: &K,
    case: &BenchmarkCase<EcosafetyInput>,
) -> BenchmarkResult
where
    K: EcosafetyKernel,
{
    let mut last_output: Option<EcosafetyOutput> = None;
    let start = Instant::now();
    for _ in 0..case.iterations {
        let out = kernel.evaluate(&case.input);
        last_output = Some(out);
    }
    let total = start.elapsed();
    let per_iter = if case.iterations == 0 {
        Duration::from_secs(0)
    } else {
        total / case.iterations as u32
    };
    let _ = last_output;
    BenchmarkResult {
        name: case.name.clone(),
        iterations: case.iterations,
        total,
        per_iter,
    }
}

/// Run a benchmark for a governance kernel using the provided case.
pub fn run_governance_benchmark<K>(
    kernel: &K,
    case: &BenchmarkCase<EcosafetyInput>,
) -> BenchmarkResult
where
    K: GovernanceKernel,
{
    let mut last_output: Option<GovernanceOutput> = None;
    let start = Instant::now();
    for _ in 0..case.iterations {
        let out = kernel.evaluate(&case.input);
        last_output = Some(out);
    }
    let total = start.elapsed();
    let per_iter = if case.iterations == 0 {
        Duration::from_secs(0)
    } else {
        total / case.iterations as u32
    };
    let _ = last_output;
    BenchmarkResult {
        name: case.name.clone(),
        iterations: case.iterations,
        total,
        per_iter,
    }
}

/// Run both ecosafety and governance benchmarks and return a suite.
pub fn run_benchmarks<EK, GK>(
    ecosafety_kernel: &EK,
    governance_kernel: &GK,
    ecosafety_cases: &[BenchmarkCase<EcosafetyInput>],
    governance_cases: &[BenchmarkCase<EcosafetyInput>],
) -> BenchmarkSuite
where
    EK: EcosafetyKernel,
    GK: GovernanceKernel,
{
    let ecosafety_results = ecosafety_cases
        .iter()
        .map(|c| run_ecosafety_benchmark(ecosafety_kernel, c))
        .collect();

    let governance_results = governance_cases
        .iter()
        .map(|c| run_governance_benchmark(governance_kernel, c))
        .collect();

    BenchmarkSuite::new(ecosafety_results, governance_results)
}

/// Format a benchmark result as a single human-readable line.
pub fn format_result_line(result: &BenchmarkResult) -> String {
    format!(
        "{name}: {iters} iters in {total_ms:.3} ms ({per_ns:.0} ns/iter, {ips:.1} iter/s)",
        name = result.name,
        iters = result.iterations,
        total_ms = result.total.as_secs_f64() * 1_000.0,
        per_ns = result.per_iter.as_nanos(),
        ips = result.iter_per_second(),
    )
}

/// Format an entire benchmark suite as multi-line text.
pub fn format_suite(suite: &BenchmarkSuite) -> String {
    let mut out = String::new();
    if !suite.ecosafety_results.is_empty() {
        out.push_str("== Ecosafety kernels ==\n");
        for r in &suite.ecosafety_results {
            out.push_str(&format_result_line(r));
            out.push('\n');
        }
    }
    if !suite.governance_results.is_empty() {
        out.push_str("== Governance kernels ==\n");
        for r in &suite.governance_results {
            out.push_str(&format_result_line(r));
            out.push('\n');
        }
    }
    out
}
