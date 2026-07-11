// filename: src/browser/duties/bee-nanoswarm-safety-duty.js
// purpose: Browser-duty module that stabilizes AI sessions and tabs
//          when querying bee corridors and nanoswarm safety.
//          It never actuates hardware; it only reads ledger state
//          and produces structured, non-spoofable context for agents.

"use strict";

import { BeeCorridorLedgerClient } from "../bee-corridor-ledger-client.js";
import { NanoswarmSafetyQuery } from "../nanoswarm-safety-query.js";

/**
 * BeeNanoswarmSafetyDuty
 * Encapsulates safety-aware behaviors that AI-native browsers or MCP agents
 * can call to anchor their reasoning in committed, bee-centric governance state.
 */
export class BeeNanoswarmSafetyDuty {
  /**
   * @param {object} options
   * @param {string} options.baseUrl - Ledger API base URL.
   * @param {function} [options.fetchImpl] - Optional fetch implementation.
   */
  constructor(options) {
    this.ledgerClient = new BeeCorridorLedgerClient(options);
    this.safetyQuery = new NanoswarmSafetyQuery(this.ledgerClient);
  }

  /**
   * Load minimal safety context for a session:
   * - list safe corridors
   * - top-K highest residual corridors (risk hotspots)
   *
   * @param {number} [topK]
   * @returns {Promise<{
   *   safeCorridors: Array<{
   *     corridorId: string,
   *     regionId: string,
   *     cellId: string,
   *     vTotal: number
   *   }>,
   *   riskHotspots: Array<{
   *     corridorId: string,
   *     vTotal: number
   *   }>
   * }>}
   */
  async loadSessionSafetySnapshot(topK = 5) {
    const corridors = await this.ledgerClient.listBeeCorridors();

    // Compute hotspots by descending residual.
    const byResidual = [];
    for (const c of corridors) {
      const rv = await this.ledgerClient.getRiskVector(c.corridorId);
      byResidual.push({ corridorId: c.corridorId, vTotal: rv.vTotal });
    }
    byResidual.sort((a, b) => b.vTotal - a.vTotal);

    const safeCorridors = await this.safetyQuery.listSafeNanoswarmCorridors();
    const riskHotspots = byResidual.slice(0, topK);

    return {
      safeCorridors,
      riskHotspots,
    };
  }

  /**
   * Provide structured context for a single corridor, suitable for
   * AI-chat agents to answer "Is corridor X safe?" questions.
   *
   * @param {string} corridorId
   * @returns {Promise<{
   *   corridorId: string,
   *   riskVector: {
   *     rContact: number,
   *     rEmf: number,
   *     rAcoustic: number,
   *     rThermal: number,
   *     rChemical: number
   *   },
   *   vTotal: number,
   *   kerDeployable: {
   *     admissible: boolean,
   *     reason: string | null
   *   },
   *   credit: {
   *     cellId: string,
   *     balanceBrc: number
   *   }
   * }>}
   */
  async getCorridorContext(corridorId) {
    const risk = await this.ledgerClient.getRiskVector(corridorId);
    const ker = await this.ledgerClient.checkKerDeployable(corridorId, "beecorridor-shard");
    const creditRaw = await this.ledgerClient.getBeeCorridorCredit(risk.corridorId);

    return {
      corridorId: risk.corridorId,
      riskVector: {
        rContact: risk.rContact,
        rEmf: risk.rEmf,
        rAcoustic: risk.rAcoustic,
        rThermal: risk.rThermal,
        rChemical: risk.rChemical,
      },
      vTotal: risk.vTotal,
      kerDeployable: ker,
      credit: {
        cellId: creditRaw.cellId,
        balanceBrc: creditRaw.balanceBrc,
      },
    };
  }
}
