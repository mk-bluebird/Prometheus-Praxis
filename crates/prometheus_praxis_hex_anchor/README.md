### Hex-Anchor DID Binding Verification

The `prometheus_praxis_hex_anchor` crate provides a non-actuating helper for verifying that an ALNv2 particle’s `evidencehex` is:

- Signed by the governance DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, and
- Backed by a zero-knowledge proof that the hidden document respects RepoManifest K,E,R non-actuating corridors.

#### Core types

- `KerPolicy` encodes corridor bounds:
  - `k_min`, `e_min`, `r_max`, and `non_actuating` flag.
- `HexAnchorPublicInputs` bundles:
  - `did`: governance DID (must equal `GOVERNANCE_DID`).
  - `pubkey_hex`: Ed25519 public key in hex bound to the DID via RepoManifest.
  - `evidencehex`: hex-encoded hash/commitment to the ALNv2 particle.
  - `sig_hex`: Ed25519 signature over `evidencehex`.
  - `policy`: `KerPolicy` describing allowed K,E,R bands.
- `HexAnchorVerificationResult` returns:
  - `did`, `evidencehex`, `policy`,
  - and booleans `ker_safe` and `non_actuating`.

#### Verification function

Use `verify_hex_anchor_did_binding` to check DID binding and zk corridor safety:

```rust
use prometheus_praxis_hex_anchor::{
    KerPolicy,
    HexAnchorPublicInputs,
    verify_hex_anchor_did_binding,
    GOVERNANCE_DID,
};

let policy = KerPolicy {
    k_min: 0.8,
    e_min: 0.9,
    r_max: 0.2,
    non_actuating: true,
};

let public_inputs = HexAnchorPublicInputs {
    did: GOVERNANCE_DID.to_string(),
    pubkey_hex: "<ed25519 pubkey in hex>".to_string(),
    evidencehex: "<commitment hex>".to_string(),
    sig_hex: "<signature hex>".to_string(),
    policy,
};

let zk_proof_bytes = vec![0x42, 0x99]; // replace with real SNARK proof bytes

let result = verify_hex_anchor_did_binding(&public_inputs, &zk_proof_bytes)?;
assert!(result.ker_safe);
assert!(result.non_actuating);
```

This function:

- Verifies the Ed25519 signature against `evidencehex` using the governance public key.
- Invokes a zk-proof verifier for the hidden particle document and K,E,R triad.
- Returns a `HexAnchorVerificationResult` suitable for ALNv2 `alnctl` audits and lane guards, without ever revealing the full document or touching actuators.

Integrate this crate into your governance tooling to ensure every corridor-tightening particle is DID-bound, KER-safe, and non-actuating by construction.
