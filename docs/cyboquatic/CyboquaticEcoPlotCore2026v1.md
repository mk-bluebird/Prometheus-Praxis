<!-- filename: docs/cyboquatic/CyboquaticEcoPlotCore2026v1.md
     repo: mk-bluebird/eco_restoration_shard
     destination: Eco-Fort/docs/cyboquatic/CyboquaticEcoPlotCore2026v1.md -->

# CyboquaticEcoPlotCore2026v1

## Purpose

- Provide a canonical per-node, per-basin ecoperjoule surface for Cyboquatic machinery.
- Encode carbonnegativeok as a discrete flag driven by corridor-aligned ecoperjoule thresholds.
- Tie Cyboquatic operation into KER residual and EcoWealth kernels through vtcontrib, kscore, escore, and rscore.

## Governance and invariants

- Rows are non-actuating ledger state; they describe observed operation, not control commands.
- carbonnegativeok = 1 only when ecoperjoule satisfies the current carbon-negative corridor for CyboquaticSurfaceCarbon.
- vtcontrib must be consistent with the residual kernel Vt and PlaneWeightsShard2026v1 for Cyboquatic planes.
- signingdid must be a valid Bostrom DID; for your work this is anchored to bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7.
- evidencehex is the hash anchor used by the provenance ledger and ALN tooling.

## Query patterns

- List carbon-negative Cyboquatic nodes in a region with their ecoperjoule and vtcontrib.
- Inspect time-series ecoperjoule trends for a single node and correlate with KER changes.
