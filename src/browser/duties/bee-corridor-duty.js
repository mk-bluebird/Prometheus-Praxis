// filename: src/browser/duties/bee-corridor-duty.js
// destination: mk-bluebird/Prometheus-Praxis/src/browser/duties/bee-corridor-duty.js
// purpose: Non-actuating browser-side guard for bee corridors and nanoswarm RF

/**
 * BeeCorridorDuty encapsulates a corridor-safe control surface for:
 *  - Sunflower placement proposals (A_min, D_max, Lyapunov descent)
 *  - Nanoswarm RF summaries (r_bee_rf <= 1)
 *
 * It is explicitly non-actuating: it never sends transactions,
 * never controls hardware, and only returns verdicts + explanations.
 */
export class BeeCorridorDuty {
  /**
   * @param {Object} options
   * @param {function(string): Promise<Object|null>} options.fetchEnvelope
   *   Async function returning a BeeEnvelope by cellId:
   *   {
   *     cellId,
   *     envelopeId,
   *     minHabitatAreaM2,           // A_min_baseline
   *     maxSunflowerDensity,        // corridor-configured D_max cap
   *     metadata                    // key/value map or plain object
   *   }
   * @param {function(string): Promise<Object[]>>} [options.fetchLedger]
   *   Async function returning BeeLedgerRecord-like entries for a key.
   *   Optional; used only for richer evidence.
   * @param {{a:number,b:number,c:number}} options.lyapunovWeights
   *   Weights for V_eco = a*P + b*B - c*S. Must satisfy a>=0,b>=0,c>=0.
   */
  constructor({ fetchEnvelope, fetchLedger, lyapunovWeights }) {
    if (typeof fetchEnvelope !== "function") {
      throw new Error("BeeCorridorDuty requires a fetchEnvelope function");
    }
    this.fetchEnvelope = fetchEnvelope;
    this.fetchLedger = typeof fetchLedger === "function" ? fetchLedger : null;

    const { a, b, c } = lyapunovWeights || {};
    if (a == null || b == null || c == null) {
      throw new Error("Lyapunov weights {a,b,c} must be provided");
    }
    if (a < 0 || b < 0 || c < 0) {
      throw new Error("Lyapunov weights must be non-negative");
    }
    if (a === 0 && b === 0 && c === 0) {
      throw new Error("At least one Lyapunov weight must be positive");
    }

    this.weights = { a, b, c };
  }

  /**
   * Check a Sunflower placement proposal against:
   *  - A_min baseline (minHabitatAreaM2) for uninterrupted habitat,
   *  - D_max = min(1 / A_min_baseline, envelope.maxSunflowerDensity),
   *  - Lyapunov descent inequality a*ΔP + b*ΔB <= c*ΔS.
   *
   * This method assumes:
   *  - deltaHabitatLossM2 >= 0,
   *  - deltaStress >= 0 (stress cannot be "negative" without strong evidence).
   *
   * @param {Object} req
   * @param {string} req.assetId
   * @param {string} req.cellId
   * @param {number} req.footprintAreaM2
   * @param {number} req.deltaHabitatLossM2   // projected loss from this placement
   * @param {number} req.deltaPollinationFlux // ΔP
   * @param {number} req.deltaBeeAbundance    // ΔB
   * @param {number} req.deltaStress          // ΔS >= 0
   * @param {number} [req.currentHabitatAreaM2]
   *   Current uninterrupted habitat area; if absent, A_min is used as a strict lower bound.
   * @returns {Promise<{allowed:boolean,reasons:string[],evidence:Object}>}
   */
  async checkSunflowerPlacement(req) {
    const reasons = [];

    if (!req || typeof req.cellId !== "string") {
      return {
        allowed: false,
        reasons: ["Invalid request: cellId is required"],
        evidence: {}
      };
    }

    const envelope = await this.fetchEnvelope(req.cellId);
    if (!envelope) {
      return {
        allowed: false,
        reasons: [`No BeeEnvelope defined for cell ${req.cellId}`],
        evidence: { envelope: null }
      };
    }

    const A_min_baseline = Number(envelope.minHabitatAreaM2) || 0;
    if (A_min_baseline <= 0) {
      reasons.push(
        `Envelope for cell ${req.cellId} has non-positive minHabitatAreaM2`
      );
    }

    const deltaLoss = Math.max(0, Number(req.deltaHabitatLossM2) || 0);
    const currentHabitat =
      req.currentHabitatAreaM2 != null
        ? Math.max(0, Number(req.currentHabitatAreaM2))
        : A_min_baseline;

    const habitatAfter = currentHabitat - deltaLoss;

    if (habitatAfter < A_min_baseline) {
      reasons.push(
        `Remaining habitat ${habitatAfter.toFixed(3)} m^2 would fall below ` +
          `baseline A_min ${A_min_baseline.toFixed(3)} m^2`
      );
    }

    // D_max = 1 / A_min_baseline, then apply stricter corridor-configured cap.
    const dMaxFormula =
      A_min_baseline > 0 ? 1 / A_min_baseline : Number.POSITIVE_INFINITY;
    const corridorDmax =
      typeof envelope.maxSunflowerDensity === "number" &&
      envelope.maxSunflowerDensity > 0
        ? envelope.maxSunflowerDensity
        : dMaxFormula;
    const D_max = Math.min(dMaxFormula, corridorDmax);

    const footprint = Number(req.footprintAreaM2) || 0;
    const densityContribution =
      footprint > 0 ? 1 / footprint : Number.POSITIVE_INFINITY;

    if (densityContribution > D_max) {
      reasons.push(
        `Proposed Sunflower density contribution ${densityContribution.toFixed(
          6
        )} exceeds D_max ${D_max.toFixed(6)}`
      );
    }

    // Lyapunov descent: a ΔP + b ΔB <= c ΔS
    const { a, b, c } = this.weights;

    const dP = Number(req.deltaPollinationFlux) || 0;
    const dB = Number(req.deltaBeeAbundance) || 0;
    const dS_raw = Number(req.deltaStress) || 0;
    const dS = dS_raw < 0 ? 0 : dS_raw; // clamp negative ΔS to 0 for safety

    const deltaV = a * dP + b * dB - c * dS;

    if (deltaV > 0) {
      reasons.push(
        `Lyapunov descent violated: ΔV = ${deltaV.toFixed(
          6
        )} > 0; requires aΔP + bΔB <= cΔS`
      );
    }

    // Optional: include ledger history to support UI/auditing.
    let ledgerRecords = [];
    if (this.fetchLedger) {
      try {
        ledgerRecords = (await this.fetchLedger(req.cellId)) || [];
      } catch {
        // Do not fail the check if ledger fetch fails; just omit history.
        ledgerRecords = [];
      }
    }

    const allowed = reasons.length === 0;

    return {
      allowed,
      reasons,
      evidence: {
        envelope,
        A_min_baseline,
        currentHabitat,
        habitatAfter,
        D_max,
        densityContribution,
        deltaV,
        ledgerRecords
      }
    };
  }

  /**
   * Check nanoswarm RF summary against corridor RF ceiling.
   *
   * This mirrors the RF risk coordinate r_bee_rf used in your nanoswarm
   * CosmWasm contract, where:
   *   r_bee_rf = clamp(emfMaxMwPerM2 / rfCeilingMwPerM2, 0, 1).
   *
   * @param {Object} summary
   * @param {string} summary.cellId
   * @param {number} summary.emfMaxMwPerM2
   * @param {number} summary.rfCeilingMwPerM2
   * @returns {{allowed:boolean,rBeeRf:number,reasons:string[]}}
   */
  checkNanoswarmRf(summary) {
    const reasons = [];

    if (!summary || typeof summary.cellId !== "string") {
      reasons.push("Invalid summary: cellId is required.");
      return { allowed: false, rBeeRf: 1.0, reasons };
    }

    const emfMax = Number(summary.emfMaxMwPerM2);
    const rfCeiling = Number(summary.rfCeilingMwPerM2);

    if (!Number.isFinite(emfMax) || !Number.isFinite(rfCeiling)) {
      reasons.push("Invalid EMF inputs (non-finite values).");
      return { allowed: false, rBeeRf: 1.0, reasons };
    }

    if (emfMax < 0 || rfCeiling <= 0) {
      reasons.push("Invalid EMF inputs (negative EMF or non-positive ceiling).");
      return { allowed: false, rBeeRf: 1.0, reasons };
    }

    const ratio = emfMax / rfCeiling;
    const rBeeRf = Math.max(0, Math.min(1, ratio));

    if (rBeeRf >= 1.0) {
      reasons.push(
        `RF risk coordinate r_bee_rf=${rBeeRf.toFixed(
          6
        )} at or above corridor ceiling`
      );
    }

    const allowed = reasons.length === 0;

    return { allowed, rBeeRf, reasons };
  }
}
