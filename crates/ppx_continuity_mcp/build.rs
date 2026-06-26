fn main() {
    // Only run in CI or when explicitly enabled.
    if std::env::var("PPX_NO_IDENTITY_CLASSIFICATION").is_err() {
        return;
    }

    // Path to the exported PPX config JSON for ALETHEION.IDENTITY.PPXNOIDENTITYCLASSIFICATION.V1.
    let cfg_path = std::path::Path::new("config/PPX-NO-IDENTITY-CLASSIFICATION-001.json");

    // Your concrete CrateSymbolProvider implementation, which usually reuses the
    // symbol/callgraph data from SMARTchain preflight.
    // Here we assume a `ppx_symbol_provider` crate exists.
    let provider = ppx_symbol_provider::FromMetadata::new().expect("symbol provider");

    ppx_continuity_kernel::ppx_validator_hook::run_ppx_no_identity_classification(
        cfg_path,
        provider,
    )
    .expect("PPX NO-IDENTITY-CLASSIFICATION invariant failed");
}
