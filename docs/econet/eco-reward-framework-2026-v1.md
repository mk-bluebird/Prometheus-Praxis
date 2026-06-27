# A Verifiably Trustworthy EcoNet: Hybrid Framework for Sovereign Rewards in Ecological Repair

This document describes the EcoNet reward framework implemented under Cybercore with Prometheus-Praxis and Perkunos-Nexus as the two primary authorities.

Key properties:

- Strict monotonicity of rewards with respect to eco-impact and KER.
- Anti-inflation via ALN-governed gamma corridors and NonRollbackProvenanceAnchor.
- Zero personal financial gain: rewards are DID-bound, non-transferable capability shards.
- Public-good-only sinks: all minted value is routed to ecological and social repair (restoration, reforestation, recycling, carbon reduction, homeless support, cancer/biomed research, data-sovereignty, chat-as-labor).
- Non-actuating computation: `econet-reward-kernel` is pure numeric; `econet-reward-ffi` is a JSON C-ABI wrapper; `econet-ledger-reward-ingest` applies ALN gating before any mint.
- Formal verification: Kani harnesses prove monotonicity and corridor compliance for `compute_rewards`.

Implementation anchors:

- Reward kernel: `crates/econet-reward-kernel`
- FFI wrapper: `crates/econet-reward-ffi`
- Ledger ingest: `crates/econet-ledger-reward-ingest`
- Reward spec shard: `qpudatashards/particles/ppx.reward.spec.v1.aln`
- Public-good sinks shard: `qpudatashards/particles/econet.public.good.sink.v1.aln`
- Prometheus-Praxis design shard: `qpudatashards/particles/prometheus.praxis.public.good.design.v1.aln`
- Design artifact shard: `qpudatashards/particles/prometheus.praxis.eco-reward-framework.v1.aln`

This report is the narrative and policy backbone for those artifacts. All future changes to the reward pipeline must reference this document and its ALN bindings, and must strengthen, never weaken, protections for neurorights, eco corridors, and zero personal gain.
