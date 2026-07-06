// FILE: src/browser/duties/cyboquatic-hardware-duty.js
// ROLE: Map cybo-aquatic machines into tile/XR overlays with RoH, EcoValue,
//       and governance/compliance badges.

"use strict";

const { TileSpaceDuty } = require("./tile-space-duty.js");
const { MarineTxDuty, computeEcoValue, checkEUNRRCompliance } = require("./marine-tx-duty.js");

/**
 * @typedef {Object} CyboquaticMachine
 * @property {string} machine_id
 * @property {string} kind                      // e.g. "RESTORATION_BARGE", "PLANTING_DRONE"
 * @property {string} tile_id                   // current tile ID
 * @property {string} corridor_id               // corridor binding
 * @property {string} mode                      // e.g. "PLANTING", "SEDIMENT_BALANCING"
 * @property {number} roh                       // current risk-of-harm estimate (0..1)
 * @property {number} eco_impact                // E component for current operation
 * @property {number} knowledge_yield           // K component
 * @property {string | null} marine_tx_id       // linked MarineRestorationTx, if any
 * @property {boolean} fpci_required            // FPIC required in current corridor
 * @property {string} status                    // "IDLE" | "ACTIVE" | "PAUSED" | "BLOCKED"
 */

/**
 * @typedef {Object} CyboquaticMachineOverlay
 * @property {string} machine_id
 * @property {string} tileId
 * @property {string} kind
 * @property {string} mode
 * @property {string} status
 * @property {number} roh
 * @property {number | null} ecoValue
 * @property {string} governanceBadge           // "SAFE", "HIGH_RISK", "BLOCKED"
 * @property {string} complianceBadge           // "COMPLIANT", "PARTIAL", "BLOCKED"
 * @property {string} reason
 */

class CyboquaticHardwareDuty {
  /**
   * @param {TileSpaceDuty} tileSpaceDuty
   * @param {MarineTxDuty} marineTxDuty
   * @param {CyboquaticMachine[]} machines
   * @param {{ alpha: number, beta: number, gamma: number }} ecoValueWeights
   * @param {number} globalRoHCeiling
   */
  constructor(tileSpaceDuty, marineTxDuty, machines, ecoValueWeights, globalRoHCeiling) {
    this.tileSpaceDuty = tileSpaceDuty;
    this.marineTxDuty = marineTxDuty;
    this.machines = machines;
    this.weights = ecoValueWeights;
    this.globalRoHCeiling = globalRoHCeiling;
    this.machinesByTile = new Map();

    for (const m of machines) {
      if (!this.machinesByTile.has(m.tile_id)) {
        this.machinesByTile.set(m.tile_id, []);
      }
      this.machinesByTile.get(m.tile_id).push(m);
    }
  }

  /**
   * Compute governance badge based on RoH and status.
   *
   * @param {CyboquaticMachine} m
   * @returns {{ badge: string, reason: string }}
   */
  computeGovernanceBadge(m) {
    if (m.status === "BLOCKED") {
      return {
        badge: "BLOCKED",
        reason: "Machine is blocked by governance kernel or manual veto.",
      };
    }

    if (m.roh > this.globalRoHCeiling) {
      return {
        badge: "BLOCKED",
        reason: `RoH ${m.roh.toFixed(2)} exceeds global ceiling ${this.globalRoHCeiling.toFixed(2)}.`,
      };
    }

    if (m.roh > this.globalRoHCeiling * 0.7) {
      return {
        badge: "HIGH_RISK",
        reason: `RoH is within ceiling but in higher-risk band for current corridor.`,
      };
    }

    return {
      badge: "SAFE",
      reason: "RoH below ceiling and no governance block flagged.",
    };
  }

  /**
   * Compute compliance badge based on linked MarineRestorationTx, FPIC, and EUNRR.
   *
   * @param {CyboquaticMachine} m
   * @returns {{ badge: string, reason: string, ecoValue: number | null }}
   */
  computeComplianceBadge(m) {
    if (!m.marine_tx_id) {
      return {
        badge: "PARTIAL",
        reason: "No MarineRestorationTx evidence bundle linked to machine.",
        ecoValue: null,
      };
    }

    // Get MarineRestorationTx from MarineTxDuty context.
    const activeTileTxs = this.marineTxDuty.getActiveTileMarineTxs();
    const tx = activeTileTxs.find(t => t.tx_id === m.marine_tx_id);
    if (!tx) {
      return {
        badge: "PARTIAL",
        reason: "Linked MarineRestorationTx not present in active tile context.",
        ecoValue: null,
      };
    }

    // FPIC: if required and status not GRANTED, block.
    if (m.fpci_required && tx.fpic_status !== "GRANTED") {
      return {
        badge: "BLOCKED",
        reason: `FPIC required but status is '${tx.fpic_status}'.`,
        ecoValue: null,
      };
    }

    // EU NRR compliance via MarineTxDuty helper.
    const eunrrEnv = this.marineTxDuty.eunrrEnvelope;
    const compliance = checkEUNRRCompliance(tx, eunrrEnv);

    const ecoValue = computeEcoValue(tx, this.weights);

    if (!compliance.compliant) {
      return {
        badge: "BLOCKED",
        reason: compliance.reason,
        ecoValue,
      };
    }

    return {
      badge: "COMPLIANT",
      reason: "Machine linked to compliant MarineRestorationTx with valid EcoValue.",
      ecoValue,
    };
  }

  /**
   * Build overlays for cybo-aquatic machines on the active tile.
   *
   * @returns {CyboquaticMachineOverlay[]}
   */
  getActiveTileMachineOverlays() {
    const tileId = this.tileSpaceDuty.getActiveTile().id;
    const machines = (this.machinesByTile.get(tileId) || []).slice();
    const overlays = [];

    for (const m of machines) {
      const gov = this.computeGovernanceBadge(m);
      const comp = this.computeComplianceBadge(m);

      overlays.push({
        machine_id: m.machine_id,
        tileId,
        kind: m.kind,
        mode: m.mode,
        status: m.status,
        roh: m.roh,
        ecoValue: comp.ecoValue,
        governanceBadge: gov.badge,
        complianceBadge: comp.badge,
        reason: `${gov.reason} ${comp.reason}`,
      });
    }

    return overlays;
  }

  /**
   * Build overlays for cybo-aquatic machines on neighbor tiles.
   *
   * @returns {CyboquaticMachineOverlay[]}
   */
  getNeighborTileMachineOverlays() {
    const overlays = [];
    const neighborIds = this.tileSpaceDuty.getActiveNeighbors();

    for (const tileId of neighborIds) {
      const machines = (this.machinesByTile.get(tileId) || []).slice();
      for (const m of machines) {
        const gov = this.computeGovernanceBadge(m);
        const comp = this.computeComplianceBadge(m);

        overlays.push({
          machine_id: m.machine_id,
          tileId,
          kind: m.kind,
          mode: m.mode,
          status: m.status,
          roh: m.roh,
          ecoValue: comp.ecoValue,
          governanceBadge: gov.badge,
          complianceBadge: comp.badge,
          reason: `${gov.reason} ${comp.reason}`,
        });
      }
    }

    return overlays;
  }
}

module.exports = {
  CyboquaticHardwareDuty,
};
