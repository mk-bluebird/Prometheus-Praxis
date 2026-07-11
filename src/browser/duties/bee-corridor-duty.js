// filename: src/browser/duties/bee-corridor-duty.js
// purpose: Non-actuating browser-side guard for bee corridors and nanoswarm RF
// target: Javasphere / AI-native browsers (agent shells, MCP, etc.)

/**
 * BeeCorridorDuty encapsulates a corridor-safe control surface
 * for:
 *  - Sunflower placement proposals (D_max, A_min, Lyapunov descent)
 *  - Nanoswarm RF summaries (r_bee_rf <= 1)
 *
 * It is explicitly non-actuating: it never sends transactions,
 * never controls hardware, and only returns verdicts + explanations.
 */
export class BeeCorridorDuty {
  /**
   * @param {Object} options
   * @param {function(string): Promise<Object>} options.fetchEnvelope
   *   Async function that returns a BeeEnvelope by cellId:
   *   { cellId, envelopeId, minHabitatAreaM2, maxSunflowerDensity, metadata }
   * @param {function(string): Promise<Object[]>} options.fetchLedger
   *   Async function that returns ledger records for a key (cellId/assetId).
   * @param {Object} options.lyapunovWeights
   *   { a: number, b: number, c: number } for V = a*P + b*B - c*S.
   */
  constructor({ fetchEnvelope, fetchLedger, lyapunovWeights }) {
    this.fetchEnvelope = fetchEnvelope;
    this.fetchLedger = fetchLedger;
    this.weights = lyapunovWeights;
  }

  /**
   * Check a Sunflower placement proposal against:
   *  - A_min (minHabitatAreaM2)
   *  - D_max = 1 / A_min and envelope.maxSunflowerDensity
   *  - Lyapunov descent inequality a*ΔP + b*ΔB <= c*ΔS
   *
   * @param {Object} req
   * @param {string} req.assetId
   * @param {string} req.cellId
   * @param {number} req.footprintAreaM2
   * @param {number} req.deltaHabitatLossM2
   * @param {number} req.deltaPollinationFlux   // ΔP
   * @param {number} req.deltaBeeAbundance      // ΔB
   * @param {number} req.deltaStress            // ΔS (>=0)
   * @returns {Promise<{allowed: boolean, reasons: string[], evidence: Object}>}
   */
  async checkSunflowerPlacement(req) {
    const reasons = [];
    const envelope = await this.fetchEnvelope(req.cellId);
    if (!envelope) {
      return {
        allowed: false,
        reasons: [`No BeeEnvelope defined for cell ${req.cellId}`],
        evidence: { envelope: null }
      };
    }

    const A_min = envelope.minHabitatAreaM2;
    const delta = req.deltaHabitatLossM2;
    const remaining = A_min - delta;

    if (remaining < A_min) {
      reasons.push(
        `Remaining habitat ${remaining.toFixed(3)} m^2 falls below A_min ` +
        `${A_min.toFixed(3)} m^2`
      );
    }

    // D_max = 1 / A_min, then take stricter of corridor-configured density.
    const dMaxFormula = A_min > 0 ? 1 / A_min : Number.POSITIVE_INFINITY;
    const dMax = Math.min(envelope.maxSunflowerDensity, dMaxFormula);

    const densityContribution =
      req.footprintAreaM2 > 0 ? 1 / req.footprintAreaM2 : Number.POSITIVE_INFINITY;

    if (densityContribution > dMax) {
      reasons.push(
        `Proposed Sunflower density contribution ${densityContribution.toFixed(6)} ` +
        `exceeds D_max ${dMax.toFixed(6)}`
      );
    }

    // Lyapunov descent: a ΔP + b ΔB <= c ΔS
    const { a, b, c } = this.weights;
    const deltaV = a * req.deltaPollinationFlux +
                   b * req.deltaBeeAbundance -
                   c * req.deltaStress;

    if (deltaV > 0) {
      reasons.push(
        `Lyapunov descent violated: ΔV = ${deltaV.toFixed(6)} > 0; ` +
        `requires aΔP + bΔB <= cΔS`
      );
    }

    const allowed = reasons.length === 0;

    return {
      allowed,
      reasons,
      evidence: {
        envelope,
        A_min,
        D_max: dMax,
        densityContribution,
        deltaV
      }
    };
  }

  /**
   * Check nanoswarm RF summary against corridor RF ceiling.
   *
   * @param {Object} summary
   * @param {string} summary.cellId
   * @param {number} summary.emfMaxMwPerM2
   * @param {number} summary.rfCeilingMwPerM2
   * @returns {{allowed: boolean, rBeeRf: number, reasons: string[]}}
   */
  checkNanoswarmRf(summary) {
    const reasons = [];
    if (summary.emfMaxMwPerM2 < 0 || summary.rfCeilingMwPerM2 <= 0) {
      reasons.push("Invalid EMF inputs (negative or zero ceiling).");
      return { allowed: false, rBeeRf: 1.0, reasons };
    }

    const rBeeRf = Math.max(
      0,
      Math.min(1, summary.emfMaxMwPerM2 / summary.rfCeilingMwPerM2)
    );

    if (rBeeRf > 1.0) {
      reasons.push(
        `RF risk coordinate r_bee_rf=${rBeeRf.toFixed(6)} exceeds corridor ceiling`
      );
    }

    const allowed = reasons.length === 0;

    return { allowed, rBeeRf, reasons };
  }
}
