// src/habitat/habitat-service.js

import { HabitatRegistry, buildRegistryFromJson } from "./habitat-registry.js";

/**
 * HabitatService provides a small, declarative API tailored for AI-chat agents.
 * All methods return plain JSON structures and avoid complex types.[file:79]
 */
export class HabitatService {
  /**
   * @param {HabitatRegistry} registry
   */
  constructor(registry) {
    this.registry = registry;
  }

  /**
   * List habitats for a chat query: jurisdiction + optional type.
   * @param {Object} params
   * @param {string} [params.jurisdictionId]
   * @param {string} [params.type]
   * @returns {Object[]} JSON-serializable habitat summaries.
   */
  listHabitats(params = {}) {
    const zones = this.registry.listHabitats(params);
    return zones.map((hz) => ({
      habitatId: hz.habitatId,
      jurisdictionId: hz.jurisdictionId,
      name: hz.name,
      type: hz.type,
      sensitivity: hz.sensitivity,
      mappingCompleteness: hz.mappingCompleteness,
      rohCeiling: hz.rohCeiling,
      speciesCount: hz.speciesLinks.length,
    }));
  }

  /**
   * Get detailed info for a single habitat, including species links and
   * latest risk snapshot, suitable for RRR explanations.[file:72][file:73]
   *
   * @param {string} habitatId
   * @returns {Object|null}
   */
  getHabitatDetail(habitatId) {
    const hz = this.registry.getHabitat(habitatId);
    if (!hz) return null;
    const snap = this.registry.getLatestRiskSnapshot(habitatId);
    return {
      habitatId: hz.habitatId,
      jurisdictionId: hz.jurisdictionId,
      name: hz.name,
      type: hz.type,
      sensitivity: hz.sensitivity,
      mappingCompleteness: hz.mappingCompleteness,
      rohCeiling: hz.rohCeiling,
      speciesLinks: hz.speciesLinks.map((link) => ({
        speciesId: link.speciesId,
        status: link.status,
        alphaImpact: link.alphaImpact,
        validationStatus: link.validationStatus,
        provenance: link.provenance,
      })),
      latestRiskSnapshot: snap
        ? {
            windowId: snap.windowId,
            rDataQuality: snap.rDataQuality,
            rBiodiversity: snap.rBiodiversity,
            rTopology: snap.rTopology,
          }
        : null,
    };
  }

  /**
   * Query habitats where a given species is significantly impacted,
   * enforcing alphaImpact and validation filters.[file:72]
   *
   * @param {Object} params
   * @param {string} params.speciesId
   * @param {number} [params.minAlphaImpact]
   * @param {boolean} [params.requireValidated]
   * @returns {Object[]} list of { habitat, speciesImpact } objects.
   */
  queryHabitatsBySpecies(params) {
    const pairs = this.registry.queryBySpecies(params);
    return pairs.map(({ habitat, link }) => ({
      habitatId: habitat.habitatId,
      habitatName: habitat.name,
      habitatType: habitat.type,
      jurisdictionId: habitat.jurisdictionId,
      sensitivity: habitat.sensitivity,
      mappingCompleteness: habitat.mappingCompleteness,
      rohCeiling: habitat.rohCeiling,
      speciesImpact: {
        speciesId: link.speciesId,
        status: link.status,
        alphaImpact: link.alphaImpact,
        validationStatus: link.validationStatus,
        provenance: link.provenance,
      },
    }));
  }

  /**
   * Get habitats safe enough (by mapping completeness and RoH ceiling)
   * to consider in a dredge planning conversation.[file:72][file:54]
   *
   * @param {Object} params
   * @param {string} [params.jurisdictionId]
   * @param {number} [params.minMappingCompleteness]
   * @param {number} [params.maxRohCeiling]
   * @returns {Object[]}
   */
  queryHabitatsForDredge(params = {}) {
    const zones = this.registry.queryForDredgePlanning(params);
    return zones.map((hz) => ({
      habitatId: hz.habitatId,
      jurisdictionId: hz.jurisdictionId,
      name: hz.name,
      type: hz.type,
      sensitivity: hz.sensitivity,
      mappingCompleteness: hz.mappingCompleteness,
      rohCeiling: hz.rohCeiling,
    }));
  }
}

/**
 * Helper: build a HabitatService directly from a JSON blob, which is how
 * most AI-chat platforms will ingest your exported habitat mapping.[file:80]
 *
 * @param {Object} json
 * @returns {HabitatService}
 */
export function createHabitatServiceFromJson(json) {
  const registry = buildRegistryFromJson(json);
  return new HabitatService(registry);
}
