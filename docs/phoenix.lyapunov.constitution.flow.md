```
flowchart TD
  defConstitution[defConstitution<br>PhoenixLyapunovConstitution2026v1]
  defRiskUHI[defRiskUHI<br>RiskVector UHI planes]
  defRiskMAR[defRiskMAR<br>RiskVector MAR planes]
  defEcoPlane[defEcoPlane<br>Ceco credit plane]

  instShardUHI[instShardUHI<br>phoenix.uhi.hex.risk.v1]
  instShardMAR[instShardMAR<br>phoenix.mar.corridor.v1]
  instWorkload[instWorkload<br>AI workload Joules]

  polInvariant[polInvariant<br>V(t+1) − V(t) ≤ −s_t]
  polNonOffset[polNonOffset<br>Non-offsettable corridor check]
  polCreditRule[polCreditRule<br>Mint Ceco only if ΔV < 0 and corridors OK]

  envDeploy[envDeploy<br>Lane: Deploy]
  envResearch[envResearch<br>Lane: ResearchOnly]
  envBlockedInv[envBlockedInv<br>Lane: BlockedByInvariant]
  envBlockedCorr[envBlockedCorr<br>Lane: BlockedByCorridor]
  envCreditMint[envCreditMint<br>Ceco > 0 (ledger only)]

  instShardUHI --> defRiskUHI
  instShardMAR --> defRiskMAR

  defConstitution --> polInvariant
  defConstitution --> polNonOffset
  defConstitution --> polCreditRule

  defRiskUHI --> polInvariant
  defRiskMAR --> polInvariant

  polInvariant --> envDeploy
  polInvariant --> envResearch
  polInvariant --> envBlockedInv

  polNonOffset --> envBlockedCorr

  instWorkload --> defEcoPlane
  defEcoPlane --> polCreditRule
  polCreditRule --> envCreditMint
```
