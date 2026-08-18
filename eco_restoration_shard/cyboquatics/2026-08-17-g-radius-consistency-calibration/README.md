# PHX-RADIUS-CONSISTENCY-CALIBRATION-20260817

Non-actuating domain (g) artifact set for cross-language blast-radius conformance
testing and constrained bank-sensitivity calibration from field-surveyed events.

Files:
- cpp/blast_radius_oracle.cpp
- java/BlastRadiusCalibration.java
- kotlin/BlastRadiusConformance.kt
- lua/blast_radius_conformance.lua
- sql/blast_radius_consistency_calibration.sql

Shared deterministic zoning specification:
  R_base = c * sqrt(Q * T)
  R_cons = R_base * (1 + 1.5 * S_b)

  SAFE     if d > R_cons
  CAUTION  if R_cons / 2 < d <= R_cons
  EXCLUDE  if 0 <= d <= R_cons / 2

All implementations:
- Parse decimal input as a finite non-negative IEEE-754 double.
- Reject Q <= 0, T <= 0, c <= 0, S_b outside [0, 1], and d < 0.
- Compute the same radius and comparison boundaries.
- Emit a canonical one-line result:
    zone|scaled_radius

The scaled radius is:
  rounded(R_cons * 1,000,000)

The integer scaling removes text-format differences when comparing outputs. The
differential test proves conformance only over its finite test corpus—not for
all possible IEEE-754 values, compiler settings, runtimes, or libraries.

Calibration:
- The Java calibrator estimates multiplier m in:
    observed_radius ~= R_base * (1 + m * S_b)
- It fits m by least squares with m constrained to [0, 1.5].
- m = 1.5 is the declared conservative default and maximum allowed by this
  artifact. The fitter does not claim that it is automatically valid for every
  Phoenix canal node.
- Use independent survey observations, held-out validation, and qualified
  engineering review before changing any field policy.
