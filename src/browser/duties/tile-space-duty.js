// FILE: src/browser/duties/tile-space-duty.js
// ROLE: Virtual tile graph and safe transitioning/querying for eco-restoration objects.
// NOTE: Non-actuating: purely data-space, no device control.

"use strict";

/**
 * @typedef {Object} TileCoord
 * @property {number} x
 * @property {number} y
 */

/**
 * @typedef {Object} VirtualTile
 * @property {string} id                - Stable tile identifier (matches TileId in ALN/Rust).
 * @property {TileCoord} coord          - Logical coordinate in the virtual grid.
 * @property {string[]} corridorIds     - CorridorEnvelope IDs applying to this tile.
 * @property {string[]} neighborIds     - Adjacent tiles by id.
 */

/**
 * @typedef {Object} RestorationTask
 * Mirrors ALE-ERM-RESTORATION-TASK-001.aln (simplified for JS).
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
 * @property {number} k_score           - 0..1
 * @property {number} e_score           - 0..1
 * @property {number} r_score           - 0..1  (RoH component, must respect ceilings)
 * @property {string} gaia_snapshot_ref
 * @property {string} boden_snapshot_ref
 */

/**
 * @typedef {Object} CorridorEnvelope
 * Mirrors ALE-GOV-CORRIDOR-CONSTRAINTS-001.aln (simplified for JS).
 * @property {string} corridor_id
 * @property {string} corridor_type     - "SOIL" | "WATERCHEM" | "HABITAT" | "HEATBUDGET"
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
 * @property {string} constraint_kind   - "HARD" | "SOFT_HIGH_PENALTY"
 * @property {string[]} treaty_ids
 */

/**
 * @typedef {Object} GaiaSentinelSnapshot
 * Mirrors ALE-GOV-GAIA-SNAPSHOT-001.aln (simplified for JS).
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
 * @property {string} autopause_reason  - "NONE" | "MOISTURE" | "HEAT_DROUGHT" | "FLOOD" | "FIRE" | "COMBINED"
 */

/**
 * @typedef {Object} GaiaCorridorThresholds
 * Mirrors ALE-GOV-GAIA-THRESHOLDS-001.aln (simplified for JS).
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
 * In-memory index of eco objects keyed by tile.
 * This keeps querying cheap and avoids unsafe cross-object coupling.
 */
class TileObjectIndex {
  constructor() {
    /** @type {Map<string, RestorationTask[]>} */
    this.tasksByTile = new Map();
    /** @type {Map<string, CorridorEnvelope[]>} */
    this.corridorsByTile = new Map();
    /** @type {Map<string, GaiaSentinelSnapshot[]>} */
    this.gaiaByTile = new Map();
    /** @type {GaiaCorridorThresholds | null} */
    this.gaiaThresholds = null;
  }

  /**
   * Attach thresholds (usually from ALN/Rust; here just a JS mirror).
   * @param {GaiaCorridorThresholds} thresholds
   */
  setGaiaThresholds(thresholds) {
    this.gaiaThresholds = { ...thresholds };
  }

  /**
   * Index tasks by tile.
   * @param {RestorationTask[]} tasks
   */
  addTasks(tasks) {
    for (const t of tasks) {
      if (!this.tasksByTile.has(t.tile_id)) {
        this.tasksByTile.set(t.tile_id, []);
      }
      this.tasksByTile.get(t.tile_id).push({ ...t });
    }
  }

  /**
   * Index corridors by tile via explicit tile binding.
   * @param {string} tileId
   * @param {CorridorEnvelope[]} corridors
   */
  addCorridorsForTile(tileId, corridors) {
    this.corridorsByTile.set(tileId, corridors.map(c => ({ ...c })));
  }

  /**
   * Index Gaia snapshots by tile.
   * @param {GaiaSentinelSnapshot[]} snapshots
   */
  addGaiaSnapshots(snapshots) {
    for (const s of snapshots) {
      if (!this.gaiaByTile.has(s.tile_id)) {
        this.gaiaByTile.set(s.tile_id, []);
      }
      this.gaiaByTile.get(s.tile_id).push({ ...s });
    }
  }

  /**
   * Query tasks on a given tile, optionally filtered by action_kind.
   * @param {string} tileId
   * @param {string | null} actionKind
   * @returns {RestorationTask[]}
   */
  getTasks(tileId, actionKind = null) {
    const tasks = this.tasksByTile.get(tileId) || [];
    if (!actionKind) return tasks.slice();
    return tasks.filter(t => t.action_kind === actionKind);
  }

  /**
   * Fetch corridors bound to a tile.
   * @param {string} tileId
   * @returns {CorridorEnvelope[]}
   */
  getCorridors(tileId) {
    const corridors = this.corridorsByTile.get(tileId) || [];
    return corridors.slice();
  }

  /**
   * Get latest Gaia snapshot for a tile (by timestamp).
   * @param {string} tileId
   * @returns {GaiaSentinelSnapshot | null}
   */
  getLatestGaiaSnapshot(tileId) {
    const snaps = this.gaiaByTile.get(tileId) || [];
    if (snaps.length === 0) return null;
    return snaps.reduce((latest, snap) =>
      snap.timestamp_utc > latest.timestamp_utc ? snap : latest
    );
  }

  /**
   * High-level eco safety summary for a tile.
   * Combines corridors, latest Gaia, and task RoH to support visualization/query.
   * @param {string} tileId
   * @returns {{
   *   tileId: string,
   *   corridors: CorridorEnvelope[],
   *   latestGaia: GaiaSentinelSnapshot | null,
   *   tasks: RestorationTask[],
   *   maxTaskRoH: number | null
   * }}
   */
  summarizeTile(tileId) {
    const tasks = this.getTasks(tileId, null);
    const corridors = this.getCorridors(tileId);
    const latestGaia = this.getLatestGaiaSnapshot(tileId);
    const maxTaskRoH =
      tasks.length === 0
        ? null
        : tasks.reduce((max, t) => (t.r_score > max ? t.r_score : max), 0);

    return {
      tileId,
      corridors,
      latestGaia,
      tasks,
      maxTaskRoH,
    };
  }
}

/**
 * TileSpaceDuty: manages virtual tiles and safe transitions between them,
 * while exposing queryable views of eco objects for the current tile and neighbors.
 */
class TileSpaceDuty {
  /**
   * @param {VirtualTile[]} tiles
   * @param {TileObjectIndex} index
   * @param {string} initialTileId
   */
  constructor(tiles, index, initialTileId) {
    /** @type {Map<string, VirtualTile>} */
    this.tiles = new Map();
    for (const t of tiles) {
      this.tiles.set(t.id, {
        id: t.id,
        coord: { ...t.coord },
        corridorIds: [...t.corridorIds],
        neighborIds: [...t.neighborIds],
      });
    }

    /** @type {TileObjectIndex} */
    this.index = index;

    if (!this.tiles.has(initialTileId)) {
      throw new Error(`Initial tile ${initialTileId} does not exist in TileSpaceDuty.`);
    }

    /** @type {string} */
    this.activeTileId = initialTileId;
  }

  /**
   * Get currently active tile.
   * @returns {VirtualTile}
   */
  getActiveTile() {
    const tile = this.tiles.get(this.activeTileId);
    if (!tile) throw new Error("Active tile missing from graph.");
    return tile;
  }

  /**
   * List neighbor tile IDs for the active tile.
   * @returns {string[]}
   */
  getActiveNeighbors() {
    return this.getActiveTile().neighborIds.slice();
  }

  /**
   * Attempt to transition to a neighbor tile.
   * Only neighbor tiles are allowed, preventing arbitrary jumps.
   * @param {string} targetTileId
   * @returns {VirtualTile}
   */
  transitionToNeighbor(targetTileId) {
    const active = this.getActiveTile();
    if (!active.neighborIds.includes(targetTileId)) {
      throw new Error(
        `Illegal tile transition from ${active.id} to ${targetTileId}: not a neighbor.`
      );
    }
    if (!this.tiles.has(targetTileId)) {
      throw new Error(`Target tile ${targetTileId} does not exist.`);
    }
    this.activeTileId = targetTileId;
    return this.getActiveTile();
  }

  /**
   * Query summarized eco-state for the active tile.
   * @returns {ReturnType<TileObjectIndex["summarizeTile"]>}
   */
  summarizeActiveTile() {
    return this.index.summarizeTile(this.activeTileId);
  }

  /**
   * Query summarized eco-state for neighbors of the active tile.
   * @returns {Array<ReturnType<TileObjectIndex["summarizeTile"]>>}
   */
  summarizeNeighborTiles() {
    const neighborIds = this.getActiveNeighbors();
    return neighborIds.map(id => this.index.summarizeTile(id));
  }

  /**
   * Get all tasks for the active tile, with optional filter by action_kind.
   * @param {string | null} actionKind
   * @returns {RestorationTask[]}
   */
  getActiveTileTasks(actionKind = null) {
    return this.index.getTasks(this.activeTileId, actionKind);
  }

  /**
   * Get latest Gaia snapshot for the active tile.
   * @returns {GaiaSentinelSnapshot | null}
   */
  getActiveTileLatestGaia() {
    return this.index.getLatestGaiaSnapshot(this.activeTileId);
  }

  /**
   * Get corridor envelopes bound to the active tile.
   * @returns {CorridorEnvelope[]}
   */
  getActiveTileCorridors() {
    return this.index.getCorridors(this.activeTileId);
  }
}

module.exports = {
  TileObjectIndex,
  TileSpaceDuty,
};
