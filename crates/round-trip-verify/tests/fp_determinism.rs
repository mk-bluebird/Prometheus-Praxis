// crates/round-trip-verify/tests/fp_determinism.rs
#[test]
fn verify_rust_c_fp_equivalence() {
    let test_cases = vec![
        (1.0, 0.3, 0.5),   // (x, y, z)
        (f32::MAX / 2.0, 0.1, 0.2),
        (f32::MIN_POSITIVE, 0.9, 0.1),
    ];
    
    for (x, y, z) in test_cases {
        let rust_result = rust_compute_risk(x, y, z);
        
        // Call C via FFI
        let c_result = unsafe {
            c_compute_risk(x, y, z)
        };
        
        // Exact binary equality required
        assert_eq!(
            rust_result.to_bits(), 
            c_result.to_bits(),
            "FP mismatch for ({}, {}, {}): Rust={:?}, C={:?}",
            x, y, z, rust_result, c_result
        );
    }
}
