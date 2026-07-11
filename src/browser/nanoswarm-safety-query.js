// filename: src/browser/nanoswarm-safety-query.js
// purpose: High-level nanoswarm safety helpers for AI agents and dashboards.
// wiring: Wraps BeeCorridorLedgerClient, adds convenience predicates and ALN summaries.

"use strict";

import { BeeCorridorLedgerClient } from "./bee-corridor-ledger-client.js";

/**
 * NanoswarmSafetyQuery
 * Provides convenience methods for answering safety questions like:
 * - "Which corridors are currently safe for nanoswarm operations?"
 * - "Why is corridor X not safe?"
 * - "Show ALN telemetry particles and attestation status for corridor Y."
 */
export class NanoswarmSafetyQuery {
  /**
   * @param {BeeCorridorLedgerClient} ledgerClient
   */
  constructor(ledgerClient) {
    if (!(ledgerClient instanceof BeeCorridorLedgerClient)) {
      throw new Error("NanoswarmSafetyQuery requires a BeeCorridorLedgerClient instance.");
    }
    this.ledger = ledgerClient;
  }

  /**
   * List all corridors that are currently safe for nanoswarm operations,
   * according to kerDeployable + non-offsettable contact + Lyapunov residual.
   *
   * @returns {Promise<Array<{
   *   corridorId: string,
   *   regionId: string,
   *   cellId: string,
   *   vTotal: number
   * }>>}
   */
  async listSafeNanoswarmCorridors() {
    const corridors = await this.ledger.listBeeCorridors();
    const safe = [];

    for (const c of corridors) {
      const assessment = await this.ledger.assessNanoswarmSafety(c.corridorId);
      if (assessment.safeForNanoswarm) {
        safe.push({
          corridorId: c.corridorId,
          regionId: c.regionId,
          cellId: c.cellId,
          vTotal: assessment.vTotal,
        });
      }
    }

    return safe;
  }

  /**
   * Explain why a given corridor is not safe for nanoswarm operations.
   * @param {string} corridorId
   * @returns {Promise<{
   *   corridorId: string,
   *   safeForNanoswarm: boolean,
   *   reasons: string[],
   *   vTotal: number,
   *   riskVector: {
   *     rContact: number,
   *     rEmf: number,
   *     rAcoustic: number,
   *     rThermal: number,
   *     rChemical: number
   *   }
   * }>}
   */
  async explainNanoswarmUnsafety(corridorId) {
    const assessment = await this.ledger.assessNanoswarmSafety(corridorId);
    return assessment;
  }

  /**
   * Fetch telemetry ALN particle summaries for a corridor,
   * including attestation and signing status. This is read-only
   * and intended for audit dashboards and AI tools.
   *
   * @param {string} corridorId
   * @returns {Promise<Array<{
   *   snapshotId: string,
   *   timestampUtc: number,
   *   deviceId: string,
   *   alnParticleId: string,
   *   attestationOk: boolean,
   *   evidenceHex: string
   * }>>}
   */
  async listTelemetryParticles(corridorId) {
    const data = await this.ledger._get("/telemetry/particles", { corridorId });
    return Array.isArray(data) ? data : [];
  }
}
