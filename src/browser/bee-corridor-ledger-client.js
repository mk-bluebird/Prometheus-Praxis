// filename: src/browser/bee-corridor-ledger-client.js
// purpose: Read-only client for querying BeeCorridorLedger and nanoswarm safety state.
// wiring: Talks to a Rust cdylib or HTTP JSON API that wraps `beecorridor-ledger`
//         and related governance crates. All methods are non-actuating.

"use strict";

/**
 * BeeCorridorLedgerClient
 * Read-only facade over the BeeCorridorLedger Rust/SQLite spine.
 * All methods return Promises and never perform writes or actuation.
 */
export class BeeCorridorLedgerClient {
  /**
   * @param {object} options
   * @param {string} options.baseUrl - Base URL for the ledger API, e.g. "/api/bee-ledger".
   * @param {function} [options.fetchImpl] - Optional fetch implementation (for node/agents).
   */
  constructor(options) {
    if (!options || typeof options.baseUrl !== "string") {
      throw new Error("BeeCorridorLedgerClient requires a baseUrl string.");
    }
    this.baseUrl = options.baseUrl.replace(/\/+$/, "");
    this.fetchImpl = options.fetchImpl || fetch;
  }

  /**
   * Internal helper to call JSON APIs with strict read-only semantics.
   * @param {string} path
   * @param {object} [params]
   * @returns {Promise<any>}
   */
  async _get(path, params = {}) {
    const url = new URL(this.baseUrl + path, window.location.origin);
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined && value !== null) {
        url.searchParams.set(key, String(value));
      }
    });

    const resp = await this.fetchImpl(url.toString(), {
      method: "GET",
      headers: {
        "Accept": "application/json",
      },
    });

    if (!resp.ok) {
      const text = await resp.text().catch(() => "");
      throw new Error(`Ledger GET ${url.pathname} failed: ${resp.status} ${text}`);
    }

    const data = await resp.json();
    return data;
  }

  /**
   * List bee corridors with basic metadata and health flags.
   * Useful for dashboards and AI agents to enumerate corridors.
   * @returns {Promise<Array<{
   *   corridorId: string,
   *   regionId: string,
   *   cellId: string,
   *   healthy: boolean,
   *   vTotal: number
   * }>>}
   */
  async listBeeCorridors() {
    const data = await this._get("/corridors");
    return Array.isArray(data) ? data : [];
  }

  /**
   * Get the latest BeeRiskVector and Lyapunov residual for a specific corridor.
   * @param {string} corridorId
   * @returns {Promise<{
   *   corridorId: string,
   *   rContact: number,
   *   rEmf: number,
   *   rAcoustic: number,
   *   rThermal: number,
   *   rChemical: number,
   *   vTotal: number
   * }>}
   */
  async getRiskVector(corridorId) {
    if (!corridorId) {
      throw new Error("getRiskVector requires corridorId.");
    }
    const data = await this._get("/corridors/risk-vector", { corridorId });
    return data;
  }

  /**
   * Query corridor bands (safe/gold/hard) for a given corridor and plane.
   * @param {string} corridorId
   * @param {string} planeName - e.g. "MechanicalContact", "Emf"
   * @returns {Promise<{
   *   corridorId: string,
   *   planeName: string,
   *   safeLo: number,
   *   safeHi: number,
   *   goldLo: number,
   *   goldHi: number,
   *   hardLo: number,
   *   hardHi: number,
   *   evidenceId: string,
   *   versionTag: string
   * }>}
   */
  async getCorridorBands(corridorId, planeName) {
    if (!corridorId || !planeName) {
      throw new Error("getCorridorBands requires corridorId and planeName.");
    }
    const data = await this._get("/corridors/bands", {
      corridorId,
      planeName,
    });
    return data;
  }

  /**
   * Check kerDeployable decision for a corridor, using committed-only state.
   * This wires directly to the Rust `kerdeployable_for_corridor` function.
   * @param {string} corridorId
   * @param {string} shardId
   * @returns {Promise<{
   *   admissible: boolean,
   *   reason: string | null
   * }>}
   */
  async checkKerDeployable(corridorId, shardId) {
    if (!corridorId || !shardId) {
      throw new Error("checkKerDeployable requires corridorId and shardId.");
    }
    const data = await this._get("/corridors/ker-deployable", {
      corridorId,
      shardId,
    });
    return data;
  }

  /**
   * Read-only view of BeeCorridor credits (BeeRestorationCredits) for a cell.
   * @param {string} cellId
   * @returns {Promise<{
   *   cellId: string,
   *   balanceBrc: number,
   *   lastUpdateUtc: number
   * }>}
   */
  async getBeeCorridorCredit(cellId) {
    if (!cellId) {
      throw new Error("getBeeCorridorCredit requires cellId.");
    }
    const data = await this._get("/credits/bee-corridor", { cellId });
    return data;
  }

  /**
   * Ask the ledger for a nanoswarm safety assessment in a corridor cell.
   * This combines risk vectors, Lyapunov residual, and governance rules
   * to produce a high-level safety verdict.
   *
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
  async assessNanoswarmSafety(corridorId) {
    if (!corridorId) {
      throw new Error("assessNanoswarmSafety requires corridorId.");
    }
    const data = await this._get("/nanoswarm/safety", { corridorId });
    return data;
  }
}
