// src/prometheus/fsm/eco-hamiltonian-lane-guard.js
// Eco-Hamiltonian lane guard for COLDCHECK → ENERGYCHECK → BIOARMED → ACTIVE

export const EcoLaneState = Object.freeze({
  COLD_CHECK: 'COLD_CHECK',
  ENERGY_CHECK: 'ENERGY_CHECK',
  BIO_ARMED: 'BIO_ARMED',
  ACTIVE: 'ACTIVE',
  FAULT: 'FAULT',
});

export class EcoHamiltonianLaneGuard {
  constructor(config) {
    this.rohCeiling = config.rohCeiling ?? 0.30;
    this.vtSlack = config.vtSlack ?? 0.0;
  }

  /**
   * Validate a candidate transition sequence under Hamiltonian + corridor gates.
   * @param {Array<string>} path - ordered states, e.g. [COLD_CHECK, ENERGY_CHECK, BIO_ARMED, ACTIVE]
   * @param {Object} telemetry - { roh, vtPrev, vtNext, tailwindValid, biosurfaceOk, hydraulicOk, lyapunovOk }
   * @returns {{allowed: boolean, reason?: string}}
   */
  validatePath(path, telemetry) {
    const canonical = [
      EcoLaneState.COLD_CHECK,
      EcoLaneState.ENERGY_CHECK,
      EcoLaneState.BIO_ARMED,
      EcoLaneState.ACTIVE,
    ];

    if (!Array.isArray(path) || path.length !== canonical.length) {
      return { allowed: false, reason: 'invalid_path_length' };
    }

    for (let i = 0; i < canonical.length; i += 1) {
      if (path[i] !== canonical[i]) {
        return { allowed: false, reason: 'non_canonical_transition' };
      }
    }

    const {
      roh,
      vtPrev,
      vtNext,
      tailwindValid,
      biosurfaceOk,
      hydraulicOk,
      lyapunovOk,
    } = telemetry;

    if (typeof roh !== 'number' || roh > this.rohCeiling) {
      return { allowed: false, reason: 'roh_exceeds_ceiling' };
    }

    if (!tailwindValid || !biosurfaceOk || !hydraulicOk || !lyapunovOk) {
      return { allowed: false, reason: 'corridor_predicate_failed' };
    }

    if (typeof vtPrev !== 'number' || typeof vtNext !== 'number') {
      return { allowed: false, reason: 'missing_vt_values' };
    }

    // Eco-Hamiltonian gate: V_next ≤ V_prev + vtSlack
    if (vtNext > vtPrev + this.vtSlack) {
      return { allowed: false, reason: 'hamiltonian_gate_violated' };
    }

    return { allowed: true };
  }
}
