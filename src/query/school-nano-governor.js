// src/query/school-nano-governor.js

/**
 * High-level query layer over SchoolNanoCorridorShard.
 * Exposes filter functions aligned with deployment_stage, lane, and governance planes.
 */

export class SchoolNanoGovernor {
  /**
   * @param {import('../aln/school-nano-corridor-parser.js').SchoolNanoCorridorShard} shard
   */
  constructor(shard) {
    this.shard = shard;
  }

  /**
   * Get all rows.
   * @returns {object[]}
   */
  listAll() {
    return this.shard.rows.slice();
  }

  /**
   * Filter by state (e.g. "TX" or "AZ").
   * @param {string} stateCode
   * @returns {object[]}
   */
  byState(stateCode) {
    const code = stateCode.toUpperCase();
    return this.shard.rows.filter(r => (r.state || '').toUpperCase() === code);
  }

  /**
   * Filter by deployment stage (STAGE0_SIM ... STAGE4_NORMALIZED).
   * @param {string|string[]} stages
   * @returns {object[]}
   */
  byStage(stages) {
    const set = new Set(
      Array.isArray(stages) ? stages : [stages]
    );
    return this.shard.rows.filter(r => set.has(r.deployment_stage));
  }

  /**
   * Filter by lane (EXP, PILOT, PROD).
   * @param {string|string[]} lanes
   * @returns {object[]}
   */
  byLane(lanes) {
    const set = new Set(
      Array.isArray(lanes) ? lanes : [lanes]
    );
    return this.shard.rows.filter(r => set.has(r.lane));
  }

  /**
   * Filter rows that are eligible for nano pilot consideration:
   * - kerdeployable == false (still gated),
   * - deployment_stage in STAGE2_INFRA or STAGE3_SCHOOL_PILOT,
   * - governance planes below specified thresholds.
   * @param {object} thresholds
   * @param {number} thresholds.rRegulatoryMax
   * @param {number} thresholds.rConsentMax
   * @param {number} thresholds.rLongtermMax
   * @returns {object[]}
   */
  nanoPilotCandidates(thresholds = {
    rRegulatoryMax: 0.10,
    rConsentMax: 0.30,
    rLongtermMax: 0.35
  }) {
    const allowedStages = new Set(['STAGE2_INFRA', 'STAGE3_SCHOOL_PILOT']);

    return this.shard.rows.filter(row => {
      if (row.kerdeployable === true) return false;
      if (!allowedStages.has(row.deployment_stage)) return false;

      const rReg = coerceNumber(row.r_regulatory);
      const rCon = coerceNumber(row.r_consent);
      const rLong = coerceNumber(row.r_longterm);

      if (rReg == null || rCon == null || rLong == null) return false;

      return (
        rReg <= thresholds.rRegulatoryMax &&
        rCon <= thresholds.rConsentMax &&
        rLong <= thresholds.rLongtermMax
      );
    });
  }

  /**
   * Filter rows where no nano overlay should be considered:
   * - nano_mode === "NONE"
   * - context_screwworm_zone === "NONE"
   * - r_parasite is already low
   * @param {number} rParasiteMax
   * @returns {object[]}
   */
  baselineOnly(rParasiteMax = 0.30) {
    return this.shard.rows.filter(row => {
      const rParasite = coerceNumber(row.r_parasite);
      return (
        row.nano_mode === 'NONE' &&
        row.context_screwworm_zone === 'NONE' &&
        rParasite != null &&
        rParasite <= rParasiteMax
      );
    });
  }

  /**
   * Zero-psychosis-risk check:
   * - nano_neuro_binding_score == 0,
   * - r_neuro == 0,
   * - corridor_neuro == "NEURO_TIGHT"
   * @returns {object[]}
   */
  zeroPsychosisRiskNano() {
    return this.shard.rows.filter(row => {
      const neuroScore = coerceNumber(row.nano_neuro_binding_score);
      const rNeuro = coerceNumber(row.r_neuro);
      return (
        row.nano_mode !== 'NONE' &&
        neuroScore === 0 &&
        rNeuro === 0 &&
        row.corridor_neuro === 'NEURO_TIGHT'
      );
    });
  }

  /**
   * Governance risk summary by district.
   * Returns a map: district -> { count, r_regulatory_max, r_consent_max, r_longterm_max }
   * @returns {Record<string, {count:number,r_regulatory_max:number,r_consent_max:number,r_longterm_max:number}>}
   */
  governanceSummaryByDistrict() {
    const summary = {};

    for (const row of this.shard.rows) {
      const key = `${row.state || ''}-${row.district || ''}`.trim();
      if (!summary[key]) {
        summary[key] = {
          count: 0,
          r_regulatory_max: 0,
          r_consent_max: 0,
          r_longterm_max: 0
        };
      }

      const entry = summary[key];
      entry.count += 1;

      const rReg = coerceNumber(row.r_regulatory) ?? 0;
      const rCon = coerceNumber(row.r_consent) ?? 0;
      const rLong = coerceNumber(row.r_longterm) ?? 0;

      if (rReg > entry.r_regulatory_max) entry.r_regulatory_max = rReg;
      if (rCon > entry.r_consent_max) entry.r_consent_max = rCon;
      if (rLong > entry.r_longterm_max) entry.r_longterm_max = rLong;
    }

    return summary;
  }

  /**
   * Find rows referencing a specific governance artifact id
   * (regulatory basis, consent documentation, or monitoring protocol).
   * @param {string} artifactId
   * @returns {object[]}
   */
  byGovernanceArtifact(artifactId) {
    const id = artifactId.trim();
    return this.shard.rows.filter(row =>
      row.regulatory_basis_id === id ||
      row.consent_artifact_id === id ||
      row.monitoring_protocol_id === id
    );
  }

  /**
   * Filter rows considered "safe to promote" by pure numerics:
   * - kerdeployable == true
   * - ker_k >= 0.90
   * - ker_e >= 0.90
   * - ker_r <= 0.13
   * - governance planes below given caps.
   * @param {object} thresholds
   * @returns {object[]}
   */
  readyForProd(thresholds = {
    rRegulatoryMax: 0.05,
    rConsentMax: 0.20,
    rLongtermMax: 0.20
  }) {
    return this.shard.rows.filter(row => {
      if (row.kerdeployable !== true) return false;

      const k = coerceNumber(row.ker_k);
      const e = coerceNumber(row.ker_e);
      const r = coerceNumber(row.ker_r);

      if (k == null || e == null || r == null) return false;
      if (!(k >= 0.90 && e >= 0.90 && r <= 0.13)) return false;

      const rReg = coerceNumber(row.r_regulatory);
      const rCon = coerceNumber(row.r_consent);
      const rLong = coerceNumber(row.r_longterm);

      if (rReg == null || rCon == null || rLong == null) return false;

      return (
        rReg <= thresholds.rRegulatoryMax &&
        rCon <= thresholds.rConsentMax &&
        rLong <= thresholds.rLongtermMax
      );
    });
  }
}

function coerceNumber(v) {
  if (typeof v === 'number') return Number.isFinite(v) ? v : null;
  if (typeof v === 'string') {
    const num = parseFloat(v);
    return Number.isNaN(num) ? null : num;
  }
  return null;
}
