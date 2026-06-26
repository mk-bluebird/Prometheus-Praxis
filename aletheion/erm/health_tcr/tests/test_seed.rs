// Path: aletheion/erm/health_tcr/tests/test_seed.rs

use proptest::test_runner::{Config, TestRunner};

/// Fixed test seed derived from DID bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7.
pub const HEALTH_TCR_TEST_SEED: u64 = 0x18sd_2u_jv_24_u_al; // replace with actual numeric

pub fn runner() -> TestRunner {
    TestRunner::new(Config {
        failure_persistence: None,
        source_file: None,
        rng_seed: Some(HEALTH_TCR_TEST_SEED),
        ..Config::default()
    })
}
