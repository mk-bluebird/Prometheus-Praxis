# PHX-KER-GOVERNANCE-BANDS-20260817

Non-actuating governance artifact set for KER policy records and aligned decision
bands.

Files:
- sql/ker_governance_bands.sql
- aln/ker_governance_bands_20260817.aln2

Maintainer DID:
  bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

KER domains:
  knowledge_factor K in [0, 1]
  eco_impact_value E in [0, 1]
  harm_risk R in [0, 1]
  ker_score = K * (E - R), therefore ker_score in [-1, 1]

Decision-band partition:
  SAFE:    0.00 <= R <= 0.25
  CAUTION: 0.25 < R < 0.60
  EXCLUDE: 0.60 <= R <= 1.00

Boundary convention:
- R = 0.25 is SAFE.
- R = 0.60 is EXCLUDE.
- CAUTION excludes both boundaries.

Equivalence claim:
- The SQL CHECK and ALN `require` predicates intentionally use the same closed
  SAFE/EXCLUDE bounds and open CAUTION interval.
- The SQL truth-table query enumerates scaled R values from 0.00 through 1.00
  in increments of 0.01 and reports any SQL/ALN classification mismatch.
- This proves equivalence only for the declared numeric predicates and the
  tested finite domain, not immutability of an external ledger, database,
  runtime, access-control system, or unauthorized agent.

The SQL database provides append-only application-level policy history through
triggers. Actual non-revocation and resistance to unauthorized modification
require deployment-specific identity verification, authorization, storage,
consensus, and audit controls outside this local SQL/ALN artifact.
