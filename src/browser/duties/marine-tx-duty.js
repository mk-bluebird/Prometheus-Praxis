// FILE: src/browser/duties/marine-tx-duty.js
// ROLE: Expose EcoValue + MarineRestorationTx summaries on marine tiles,
//       and compute EU NRR compliance badges for virtual ocean corridors.

"use strict";

const { TileSpaceDuty } = require("./tile-space-duty.js");

/**
 * @typedef {Object} EcoValueSnapshot
 * Mirrors ALE-METRICS-ECO-VALUE-001.aln (simplified for JS).
 * @property {string} eco_value_id
 * @property {string} subject_kind      - "TASK" | "MARINE_TX"
 * @property {string} subject_ref       - TaskId or TxId
 * @property {{ k: number, e: number, r: number }} ker
 * @property {{ alpha: number, beta: number, gamma: number }} weights
 * @property {number} eco_value
 */

/**
 * @typedef {Object} MarineRestorationTx
 * Mirrors ALE-TRUST-MARINE-RESTORATION-TX-001.aln (simplified for JS).
 * @property {string} tx_id
 * @property {string} campaign_id
 * @property {string} deployment_region
 * @property {string} birthsign_id
 * @property {string[]} treaty_ids
 * @property {string} eunrr_regulation_id
 * @property {string[]} articles_checked
 * @property {string} fpic_status             - "NOT_APPLICABLE" | "PENDING" | "GRANTED" | "DENIED"
 * @property {string} fpic_evidence_ref
 * @property {number} eco_impact              - E, 0..1
 * @property {number} risk_of_harm            - H, 0..1
 * @property {number} knowledge_yield         - K, 0..1
 * @property {string} eco_value_id
 * @property {string} verifier_did
 * @property {string} verifier_role           - "INDEPENDENT_AUDITOR" | "GOVERNANCE_BODY" | "COMMUNITY_REP"
 * @property {string} verification_timestamp_utc
 * @property {string} chain_id
 * @property {string} hex_trace
 * @property {string} tile_id                 - Tile this deployment is associated with.
 */

/**
 * @typedef {Object} EUNRRComplianceEnvelope
 * Machine-readable EU NRR requirements (simplified).
 * @property {string} policy_id
 * @property {string} jurisdiction            - "EU"
 * @property {string} regulation_id           - "EU-NRR-2023-XXX"
 * @property {string[]} required_articles
 * @property {string[]} article_minimum_set
 * @property {boolean} fpic_required_for_indigenous
 * @property {string[]} fpic_allowed_statuses_for_actuation
 * @property {string[]} allowed_verifier_roles
 * @property {boolean} require_dual_verification
 */

/**
 * @typedef {Object} MarineTileBadge
 * @property {string} tileId
 * @property {string} badge             - "COMPLIANT" | "PARTIAL" | "BLOCKED"
 * @property {string} reason
 * @property {number | null} maxEcoValue
 * @property {MarineRestorationTx[]} txs
 */

/**
 * Check EU NRR compliance for a single MarineRestorationTx
 * against the EUNRRComplianceEnvelope.
 *
 * Returns an object with boolean flags and human-readable reason.
 *
 * @param {MarineRestorationTx} tx
 * @param {EUNRRComplianceEnvelope} env
 * @returns {{ compliant: boolean, reason: string }}
 */
function checkEUNRRCompliance(tx, env) {
  // Articles
  const missingArticles = env.article_minimum_set.filter(
    art => !tx.articles_checked.includes(art)
  );
  if (missingArticles.length > 0) {
    return {
      compliant: false,
      reason: `Missing required EU NRR articles: ${missingArticles.join(", ")}`,
    };
  }

  // FPIC
  const hasIndigenousTreaty = tx.treaty_ids.some(id => id.toUpperCase().includes("INDIGENOUS"));
  if (env.fpic_required_for_indigenous && hasIndigenousTreaty) {
    if (!env.fpic_allowed_statuses_for_actuation.includes(tx.fpic_status)) {
      return {
        compliant: false,
        reason: `FPIC status '${tx.fpic_status}' not allowed for Indigenous treaties.`,
      };
    }
  }

  // Verifier role
  if (!env.allowed_verifier_roles.includes(tx.verifier_role)) {
    return {
      compliant: false,
      reason: `Verifier role '${tx.verifier_role}' not allowed by EUNRR envelope.`,
    };
  }

  // Dual verification (simplified: we treat chain_id or hex_trace patterns externally).
  // Here we just acknowledge requirement; full multi-verifier enforcement can be
  // handled at chain level.
  if (env.require_dual_verification) {
    // In a richer implementation, you'd look up additional signatures / verifier records.
    // For now, we assume single-verifier and mark partial.
    return {
      compliant: false,
      reason: "Dual verification required but not enforced in this JS layer.",
    };
  }

  return { compliant: true, reason: "All EUNRR checks passed in JS layer." };
}

/**
 * Compute EcoValue locally for a MarineRestorationTx using weights.
 * This is a mirror of the ALN/Rust formula and should be used only
 * for visualization; governance decisions must rely on ALN/Rust.
 *
 * @param {MarineRestorationTx} tx
 * @param {{ alpha: number, beta: number, gamma: number }} weights
 * @returns {number}
 */
function computeEcoValue(tx, weights) {
  const { eco_impact: e, risk_of_harm: h, knowledge_yield: k } = tx;
  const { alpha, beta, gamma } = weights;
  return alpha * e - beta * h + gamma * k;
}

/**
 * MarineTxDuty decorates TileSpaceDuty with marine restoration summaries.
 */
class MarineTxDuty {
  /**
   * @param {TileSpaceDuty} tileSpaceDuty
   * @param {MarineRestorationTx[]} marineTxs
   * @param {EcoValueSnapshot[]} ecoValueSnapshots
   * @param {EUNRRComplianceEnvelope} eunrrEnvelope
   */
  constructor(tileSpaceDuty, marineTxs, ecoValueSnapshots, eunrrEnvelope) {
    this.tileSpaceDuty = tileSpaceDuty;
    this.marineTxsByTile = new Map();
    this.ecoValueByTxId = new Map();
    this.eunrrEnvelope = eunrrEnvelope;

    for (const tx of marineTxs) {
      if (!this.marineTxsByTile.has(tx.tile_id)) {
        this.marineTxsByTile.set(tx.tile_id, []);
      }
      this.marineTxsByTile.get(tx.tile_id).push(tx);
    }

    for (const snap of ecoValueSnapshots) {
      if (snap.subject_kind === "MARINE_TX") {
        this.ecoValueByTxId.set(snap.subject_ref, snap);
      }
    }
  }

  /**
   * Get MarineRestorationTx entries bound to the active tile.
   * @returns {MarineRestorationTx[]}
   */
  getActiveTileMarineTxs() {
    const tileId = this.tileSpaceDuty.getActiveTile().id;
    return (this.marineTxsByTile.get(tileId) || []).slice();
  }

  /**
   * Summarize EcoValue and EU NRR badge for the active tile.
   *
   * @returns {MarineTileBadge}
   */
  summarizeActiveMarineTile() {
    const tileId = this.tileSpaceDuty.getActiveTile().id;
    const txs = this.getActiveTileMarineTxs();

    if (txs.length === 0) {
      return {
        tileId,
        badge: "PARTIAL",
        reason: "No marine restoration transactions for this tile.",
        maxEcoValue: null,
        txs: [],
      };
    }

    let maxEcoValue = null;
    let worstReason = null;
    let allCompliant = true;

    for (const tx of txs) {
      const compliance = checkEUNRRCompliance(tx, this.eunrrEnvelope);
      if (!compliance.compliant) {
        allCompliant = false;
        worstReason = compliance.reason;
      }

      const snap = this.ecoValueByTxId.get(tx.tx_id);
      let ecoValue = null;
      if (snap) {
        ecoValue = snap.eco_value;
      } else {
        // Fallback to local computation using default weights from envelope context.
        const defaultWeights = {
          alpha: 0.4,
          beta: 0.5,
          gamma: 0.1,
        };
        ecoValue = computeEcoValue(tx, defaultWeights);
      }

      if (ecoValue !== null) {
        if (maxEcoValue === null || ecoValue > maxEcoValue) {
          maxEcoValue = ecoValue;
        }
      }
    }

    let badge;
    let reason;

    if (allCompliant && maxEcoValue !== null) {
      badge = "COMPLIANT";
      reason = "Tile has marine deployments passing EU NRR checks with valid EcoValue.";
    } else if (!allCompliant) {
      badge = "BLOCKED";
      reason =
        worstReason ||
        "At least one marine deployment fails EU NRR compliance checks for this tile.";
    } else {
      badge = "PARTIAL";
      reason = "EcoValue available but some EU NRR conditions not fully met.";
    }

    return {
      tileId,
      badge,
      reason,
      maxEcoValue,
      txs,
    };
  }

  /**
   * Summarize marine badges for neighbor tiles of the current tile.
   *
   * @returns {MarineTileBadge[]}
   */
  summarizeNeighborMarineTiles() {
    const neighbors = this.tileSpaceDuty.getActiveNeighbors();
    const badges = [];

    for (const tileId of neighbors) {
      const txs = (this.marineTxsByTile.get(tileId) || []).slice();
      if (txs.length === 0) {
        badges.push({
          tileId,
          badge: "PARTIAL",
          reason: "No marine restoration transactions for this neighbor tile.",
          maxEcoValue: null,
          txs: [],
        });
        continue;
      }

      let maxEcoValue = null;
      let worstReason = null;
      let allCompliant = true;

      for (const tx of txs) {
        const compliance = checkEUNRRCompliance(tx, this.eunrrEnvelope);
        if (!compliance.compliant) {
          allCompliant = false;
          worstReason = compliance.reason;
        }

        const snap = this.ecoValueByTxId.get(tx.tx_id);
        let ecoValue = null;
        if (snap) {
          ecoValue = snap.eco_value;
        } else {
          const defaultWeights = {
            alpha: 0.4,
            beta: 0.5,
            gamma: 0.1,
          };
          ecoValue = computeEcoValue(tx, defaultWeights);
        }

        if (ecoValue !== null) {
          if (maxEcoValue === null || ecoValue > maxEcoValue) {
            maxEcoValue = ecoValue;
          }
        }
      }

      let badge;
      let reason;

      if (allCompliant && maxEcoValue !== null) {
        badge = "COMPLIANT";
        reason =
          "Neighbor tile has marine deployments passing EU NRR checks with valid EcoValue.";
      } else if (!allCompliant) {
        badge = "BLOCKED";
        reason =
          worstReason ||
          "At least one marine deployment fails EU NRR compliance checks for this neighbor tile.";
      } else {
        badge = "PARTIAL";
        reason = "EcoValue available but some EU NRR conditions not fully met.";
      }

      badges.push({
        tileId,
        badge,
        reason,
        maxEcoValue,
        txs,
      });
    }

    return badges;
  }
}

module.exports = {
  MarineTxDuty,
  checkEUNRRCompliance,
  computeEcoValue,
};
