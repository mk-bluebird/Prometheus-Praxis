// src/habitat/habitat-schema.js

/**
 * Core schema for habitat and species data, designed for AI-chat consumption
 * and RoH-aware governance. All numeric risk scores are normalized to [0, 1].
 */

export class HabitatZone {
  /**
   * @param {Object} cfg
   * @param {string} cfg.habitatId - Stable ID (e.g. "PHX-SALT-SPAWN-001").
   * @param {string} cfg.jurisdictionId - e.g. "PhoenixAZCAP".
   * @param {string} cfg.name - Human-readable name.
   * @param {string} cfg.type - e.g. "spawning", "nursery", "refugia", "corridor".
   * @param {Object} cfg.geometry - GeoJSON-like footprint (polygon or multipolygon).
   * @param {number} cfg.sensitivity - Overall sensitivity 0..1 (higher = more fragile).
   * @param {number} cfg.mappingCompleteness - Coverage quality 0..1.
   * @param {number} cfg.rohCeiling - Local RoH ceiling (≤ 0.30).
   * @param {Object[]} cfg.speciesLinks - Array of HabitatSpeciesLink configs.
   */
  constructor(cfg) {
    this.habitatId = cfg.habitatId;
    this.jurisdictionId = cfg.jurisdictionId;
    this.name = cfg.name;
    this.type = cfg.type;
    this.geometry = cfg.geometry;
    this.sensitivity = clamp01(cfg.sensitivity);
    this.mappingCompleteness = clamp01(cfg.mappingCompleteness);
    this.rohCeiling = Math.min(cfg.rohCeiling ?? 0.30, 0.30);
    this.speciesLinks = (cfg.speciesLinks || []).map(
      (linkCfg) => new HabitatSpeciesLink({ ...linkCfg, habitatId: cfg.habitatId })
    );
  }
}

/**
 * Links a species to a habitat zone, with alpha (impact strength) coefficients,
 * mirroring your SKO→species k,p pattern.[file:72]
 */
export class HabitatSpeciesLink {
  /**
   * @param {Object} cfg
   * @param {string} cfg.habitatId
   * @param {string} cfg.speciesId - e.g. "GilaTopminnow".
   * @param {string} cfg.status - e.g. "endangered", "vulnerable", "common".
   * @param {number} cfg.alphaImpact - 0..1 impact strength in this habitat.
   * @param {string} [cfg.provenance] - Citations / notes.
   * @param {("draft"|"validated")} [cfg.validationStatus] - Draft vs validated.
   */
  constructor(cfg) {
    this.habitatId = cfg.habitatId;
    this.speciesId = cfg.speciesId;
    this.status = cfg.status;
    this.alphaImpact = clamp01(cfg.alphaImpact);
    this.provenance = cfg.provenance || "";
    this.validationStatus = cfg.validationStatus || "draft";
  }

  isValidated() {
    return this.validationStatus === "validated";
  }
}

/**
 * Raw sensor→risk coordinate mapping for a habitat zone, so agents can reason
 * about r_dataQuality, r_biodiversity, r_topology for dredge windows.[file:73]
 */
export class HabitatRiskSnapshot {
  /**
   * @param {Object} cfg
   * @param {string} cfg.habitatId
   * @param {string} cfg.windowId - e.g. time-window identifier.
   * @param {number} cfg.rDataQuality - Model & sensor quality 0..1.
   * @param {number} cfg.rBiodiversity - Biodiversity stress 0..1.
   * @param {number} cfg.rTopology - Mapping/topology gaps 0..1.
   */
  constructor(cfg) {
    this.habitatId = cfg.habitatId;
    this.windowId = cfg.windowId;
    this.rDataQuality = clamp01(cfg.rDataQuality);
    this.rBiodiversity = clamp01(cfg.rBiodiversity);
    this.rTopology = clamp01(cfg.rTopology);
  }
}

function clamp01(value) {
  const v = Number.isFinite(value) ? value : 0;
  if (v < 0) return 0;
  if (v > 1) return 1;
  return v;
}
