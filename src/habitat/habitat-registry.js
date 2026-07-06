// src/habitat/habitat-registry.js

import { HabitatZone, HabitatSpeciesLink, HabitatRiskSnapshot } from "./habitat-schema.js";

/**
 * Central in-memory registry for habitats, species links, and risk snapshots.
 * Can be backed by SQLite or JSON on disk in production.[file:80]
 */
export class HabitatRegistry {
  constructor() {
    /** @type {Map<string, HabitatZone>} */
    this.habitats = new Map();
    /** @type {Map<string, HabitatRiskSnapshot[]>} */
    this.riskSnapshots = new Map();
  }

  /**
   * Add or replace a habitat zone.
   * @param {HabitatZone} zone
   */
  upsertHabitat(zone) {
    if (!(zone instanceof HabitatZone)) {
      throw new Error("upsertHabitat expects a HabitatZone instance.");
    }
    this.habitats.set(zone.habitatId, zone);
  }

  /**
   * Append a risk snapshot for a habitat.
   * @param {HabitatRiskSnapshot} snapshot
   */
  addRiskSnapshot(snapshot) {
    if (!(snapshot instanceof HabitatRiskSnapshot)) {
      throw new Error("addRiskSnapshot expects a HabitatRiskSnapshot instance.");
    }
    const list = this.riskSnapshots.get(snapshot.habitatId) || [];
    list.push(snapshot);
    this.riskSnapshots.set(snapshot.habitatId, list);
  }

  /**
   * Get all habitats, optionally filtered by jurisdiction and type.
   * @param {Object} [filter]
   * @param {string} [filter.jurisdictionId]
   * @param {string} [filter.type]
   * @returns {HabitatZone[]}
   */
  listHabitats(filter = {}) {
    const { jurisdictionId, type } = filter;
    return Array.from(this.habitats.values()).filter((hz) => {
      if (jurisdictionId && hz.jurisdictionId !== jurisdictionId) return false;
      if (type && hz.type !== type) return false;
      return true;
    });
  }

  /**
   * Get a single habitat by ID.
   * @param {string} habitatId
   * @returns {HabitatZone | null}
   */
  getHabitat(habitatId) {
    return this.habitats.get(habitatId) || null;
  }

  /**
   * Query habitats by species and minimum alpha impact.
   * Used by agents to ask “where is species X most exposed?”.[file:72]
   *
   * @param {Object} filter
   * @param {string} filter.speciesId
   * @param {number} [filter.minAlphaImpact] - 0..1, default 0.3
   * @param {boolean} [filter.requireValidated] - only validated links
   * @returns {Array<{ habitat: HabitatZone, link: HabitatSpeciesLink }>}
   */
  queryBySpecies(filter) {
    const {
      speciesId,
      minAlphaImpact = 0.3,
      requireValidated = true,
    } = filter;
    const result = [];
    for (const habitat of this.habitats.values()) {
      for (const link of habitat.speciesLinks) {
        if (link.speciesId !== speciesId) continue;
        if (requireValidated && !link.isValidated()) continue;
        if (link.alphaImpact < minAlphaImpact) continue;
        result.push({ habitat, link });
      }
    }
    return result;
  }

  /**
   * Query habitats suitable for dredge planning, enforcing RoH ≤ 0.30 and
   * minimum mapping completeness.[file:72][file:54]
   *
   * @param {Object} filter
   * @param {string} [filter.jurisdictionId]
   * @param {number} [filter.minMappingCompleteness] - 0..1, default 0.7
   * @param {number} [filter.maxRohCeiling] - hard ceiling, default 0.30
   * @returns {HabitatZone[]}
   */
  queryForDredgePlanning(filter = {}) {
    const {
      jurisdictionId,
      minMappingCompleteness = 0.7,
      maxRohCeiling = 0.30,
    } = filter;
    return Array.from(this.habitats.values()).filter((hz) => {
      if (jurisdictionId && hz.jurisdictionId !== jurisdictionId) return false;
      if (hz.mappingCompleteness < minMappingCompleteness) return false;
      if (hz.rohCeiling > maxRohCeiling) return false;
      return true;
    });
  }

  /**
   * Get latest risk snapshot for a habitat, for RRR introspection.[file:73]
   *
   * @param {string} habitatId
   * @returns {HabitatRiskSnapshot | null}
   */
  getLatestRiskSnapshot(habitatId) {
    const list = this.riskSnapshots.get(habitatId);
    if (!list || list.length === 0) return null;
    return list[list.length - 1];
  }
}

/**
 * Convenience: build a registry from a plain JSON payload, so AI chat
 * platforms can load exported habitat data easily.[file:80]
 *
 * @param {Object} json
 * @returns {HabitatRegistry}
 */
export function buildRegistryFromJson(json) {
  const registry = new HabitatRegistry();
  const habitats = Array.isArray(json.habitats) ? json.habitats : [];
  for (const hzCfg of habitats) {
    const zone = new HabitatZone(hzCfg);
    registry.upsertHabitat(zone);
  }
  const snapshots = Array.isArray(json.riskSnapshots) ? json.riskSnapshots : [];
  for (const snapCfg of snapshots) {
    const snap = new HabitatRiskSnapshot(snapCfg);
    registry.addRiskSnapshot(snap);
  }
  return registry;
}
