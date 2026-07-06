// FILE: src/browser/duties/tile-space-adapter.js
// ROLE: Adapter/registry that builds TileObjectIndex + TileSpaceDuty from JSON payloads.
// NOTE: Non-actuating, browser/XR-safe: purely virtual-space and query orchestration.

"use strict";

const { TileObjectIndex, TileSpaceDuty } = require("./tile-space-duty.js");

/**
 * @typedef {Object} TilePayload
 * @property {string} id
 * @property {{ x: number, y: number }} coord
 * @property {string[]} corridorIds
 * @property {string[]} neighborIds
 */

/**
 * @typedef {Object} RestorationTaskPayload
 * @property {string} restoration_task_id
 * @property {string} scenario_id
 * @property {string} tile_id
 * @property {string} birthsign_id
 * @property {string} corridor_id
 * @property {string} biotictreaty_id
 * @property {string} action_kind
 * @property {string[]} species_mix
 * @property {number} area_m2
 * @property {string} time_window_start
 * @property {string} time_window_end
 * @property {number} k_score
 * @property {number} e_score
 * @property {number} r_score
 * @property {string} gaia_snapshot_ref
 * @property {string} boden_snapshot_ref
 */

/**
 * @typedef {Object} CorridorEnvelopePayload
 * @property {string} corridor_id
 * @property {string} corridor_type
 * @property {string} region_id
 * @property {number} soil_moisture_min
 * @property {number} soil_moisture_max
 * @property {number} soil_ph_min
 * @property {number} soil_ph_max
 * @property {number} salinity_min
 * @property {number} salinity_max
 * @property {number} nutrient_min
 * @property {number} nutrient_max
 * @property {number} water_table_min
 * @property {number} water_table_max
 * @property {number} contamination_index_max
 * @property {number} habitat_continuity_min
 * @property {number} biodiversity_floor
 * @property {number} roh_ceiling_local
 * @property {string} constraint_kind
 * @property {string[]} treaty_ids
 * @property {string[]} tile_ids              - Optional: tiles this corridor applies to.
 */

/**
 * @typedef {Object} GaiaSentinelSnapshotPayload
 * @property {string} snapshot_id
 * @property {string} tile_id
 * @property {string} timestamp_utc
 * @property {number} soil_moisture_idx
 * @property {number} drought_idx
 * @property {number} heat_budget_idx
 * @property {number} flood_risk_idx
 * @property {number} fire_risk_idx
 * @property {boolean} moisture_below_floor
 * @property {boolean} heat_budget_over_limit
 * @property {boolean} drought_above_threshold
 * @property {boolean} flood_risk_high
 * @property {boolean} fire_risk_high
 * @property {string[]} corridor_violation_ids
 * @property {string} autopause_reason
 */

/**
 * @typedef {Object} GaiaCorridorThresholdsPayload
 * @property {string} policy_id
 * @property {string} jurisdiction
 * @property {number} soil_moisture_pause_threshold
 * @property {number} soil_moisture_days_below_for_pause
 * @property {number} heat_budget_pause_threshold
 * @property {number} drought_idx_pause_threshold
 * @property {number} flood_risk_pause_threshold
 * @property {number} fire_risk_pause_threshold
 * @property {number} max_consecutive_moisture_breach_days
 * @property {number} max_consecutive_heat_drought_breach_days
 * @property {number} max_consecutive_flood_breach_events
 * @property {number} max_consecutive_fire_breach_events
 * @property {string} preflight_rule_id
 * @property {string[]} target_workflow_ids
 */

/**
 * @typedef {Object} TileSpacePayload
 * JSON payload shape expected from Rust/ALN eco-restoration APIs.
 * @property {TilePayload[]} tiles
 * @property {RestorationTaskPayload[]} restorationTasks
 * @property {CorridorEnvelopePayload[]} corridors
 * @property {GaiaSentinelSnapshotPayload[]} gaiaSnapshots
 * @property {GaiaCorridorThresholdsPayload | null} gaiaThresholds
 * @property {string} initialTileId
 */

/**
 * Validate basic payload shape and throw descriptive errors
 * if anything critical is missing. This keeps XR/AI-browser
 * duty environments stable.
 *
 * @param {TileSpacePayload} payload
 */
function validateTileSpacePayload(payload) {
  if (!payload || typeof payload !== "object") {
    throw new Error("TileSpacePayload must be a non-null object.");
  }

  if (!Array.isArray(payload.tiles) || payload.tiles.length === 0) {
    throw new Error("TileSpacePayload.tiles must be a non-empty array.");
  }

  if (typeof payload.initialTileId !== "string") {
    throw new Error("TileSpacePayload.initialTileId must be a string.");
  }

  const tileIds = new Set(payload.tiles.map(t => t.id));
  if (!tileIds.has(payload.initialTileId)) {
    throw new Error(
      `TileSpacePayload.initialTileId '${payload.initialTileId}' does not exist in tiles array.`
    );
  }

  if (!Array.isArray(payload.restorationTasks)) {
    throw new Error("TileSpacePayload.restorationTasks must be an array.");
  }
  if (!Array.isArray(payload.corridors)) {
    throw new Error("TileSpacePayload.corridors must be an array.");
  }
  if (!Array.isArray(payload.gaiaSnapshots)) {
    throw new Error("TileSpacePayload.gaiaSnapshots must be an array.");
  }
}

/**
 * Build a TileObjectIndex from the incoming payload.
 *
 * @param {TileSpacePayload} payload
 * @returns {TileObjectIndex}
 */
function buildTileObjectIndex(payload) {
  const index = new TileObjectIndex();

  // Attach Gaia thresholds if present.
  if (payload.gaiaThresholds) {
    index.setGaiaThresholds(payload.gaiaThresholds);
  }

  // Index restoration tasks.
  index.addTasks(payload.restorationTasks);

  // Index corridors per tile.
  // Corridors may list explicit tile_ids; if not, we skip binding.
  const corridorsByTile = new Map();
  for (const corridor of payload.corridors) {
    const tileIds = Array.isArray(corridor.tile_ids) ? corridor.tile_ids : [];
    for (const tileId of tileIds) {
      if (!corridorsByTile.has(tileId)) {
        corridorsByTile.set(tileId, []);
      }
      corridorsByTile.get(tileId).push(corridor);
    }
  }

  corridorsByTile.forEach((corrs, tileId) => {
    index.addCorridorsForTile(tileId, corrs);
  });

  // Index Gaia snapshots.
  index.addGaiaSnapshots(payload.gaiaSnapshots);

  return index;
}

/**
 * Convert raw tile payloads into VirtualTile objects suitable
 * for TileSpaceDuty.
 *
 * @param {TilePayload[]} tiles
 * @returns {import("./tile-space-duty.js").VirtualTile[]}
 */
function buildVirtualTiles(tiles) {
  return tiles.map(t => ({
    id: t.id,
    coord: {
      x: Number.isFinite(t.coord?.x) ? t.coord.x : 0,
      y: Number.isFinite(t.coord?.y) ? t.coord.y : 0,
    },
    corridorIds: Array.isArray(t.corridorIds) ? [...t.corridorIds] : [],
    neighborIds: Array.isArray(t.neighborIds) ? [...t.neighborIds] : [],
  }));
}

/**
 * Main factory: create a TileSpaceDuty instance from a JSON payload.
 *
 * @param {TileSpacePayload} payload
 * @returns {TileSpaceDuty}
 */
function createTileSpaceDutyFromPayload(payload) {
  validateTileSpacePayload(payload);

  const index = buildTileObjectIndex(payload);
  const virtualTiles = buildVirtualTiles(payload.tiles);

  const duty = new TileSpaceDuty(virtualTiles, index, payload.initialTileId);
  return duty;
}

/**
 * Example helper: build payload from separate JSON blobs
 * (e.g., fetched from different endpoints) and attach an initial tile.
 *
 * This is useful in AI-native browsers where tasks, corridors,
 * and Gaia data may arrive separately.
 *
 * @param {TilePayload[]} tiles
 * @param {RestorationTaskPayload[]} tasks
 * @param {CorridorEnvelopePayload[]} corridors
 * @param {GaiaSentinelSnapshotPayload[]} gaiaSnapshots
 * @param {GaiaCorridorThresholdsPayload | null} gaiaThresholds
 * @param {string} initialTileId
 * @returns {TileSpacePayload}
 */
function composeTileSpacePayload(
  tiles,
  tasks,
  corridors,
  gaiaSnapshots,
  gaiaThresholds,
  initialTileId
) {
  return {
    tiles,
    restorationTasks: tasks,
    corridors,
    gaiaSnapshots,
    gaiaThresholds,
    initialTileId,
  };
}

module.exports = {
  createTileSpaceDutyFromPayload,
  composeTileSpacePayload,
};
