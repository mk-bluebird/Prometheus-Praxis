// filename: src/browser/agents/sunflower-agent.js
// destination: mk-bluebird/Prometheus-Praxis/src/browser/agents/sunflower-agent.js

import { BeeCorridorDuty } from "../duties/bee-corridor-duty.js";
import { fetchEnvelopeFromApi, fetchLedgerFromApi } from "../infra/eco-api.js";

const duty = new BeeCorridorDuty({
  fetchEnvelope: fetchEnvelopeFromApi,
  fetchLedger: fetchLedgerFromApi,
  lyapunovWeights: { a: 1.0, b: 1.0, c: 1.0 } // must also be reflected in PlaneWeightsShard
});

export async function proposeSunflowerPlacement(agentProposal) {
  const verdict = await duty.checkSunflowerPlacement(agentProposal);

  if (!verdict.allowed) {
    // Render reasons in UI / MCP response; do not send any transaction.
    return {
      approved: false,
      reasons: verdict.reasons,
      evidence: verdict.evidence
    };
  }

  // At this point, browser duty says it is corridor-safe.
  // Actual actuation is delegated to a Rust/WASM client or CosmWasm contract.
  // This layer MUST NOT send the transaction; it should only return
  // an "intent" that a separate, governed component can inspect.
  return {
    approved: true,
    reasons: [],
    evidence: verdict.evidence
  };
}
