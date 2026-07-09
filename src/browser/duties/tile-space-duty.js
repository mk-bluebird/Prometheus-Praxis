// src/browser/duties/tile-space-duty.js
// ROLE: Virtual tile graph and safe transitioning/querying for eco-restoration objects.
// NOTE: Non-actuating: purely data-space, no device control.

"use strict";

import EcoStationRegistryDuty from "./eco-station-registry.js";

/**
 * @typedef {Object} TileCoord
 * @property {number} x
 * @property {number} y
 */

/**
 * @typedef {Object} VirtualTile
 * @property {string} id
 * @property {TileCoord} coord
 * @property {string[]} corridorIds
 * @property {string[]} neighborIds
 */

/**
 * @typedef {Object} RestorationTask
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
 * @typedef {Object} CorridorEnvelope
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
 */

/**
 * @typedef {Object} GaiaSentinelSnapshot
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
 * @typedef {Object} GaiaCorridorThresholds
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
 */
export class LocalTileObjectIndex {
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

  setGaiaThresholds(thresholds) {
    this.gaiaThresholds = { ...thresholds };
  }

  addTasks(tasks) {
    for (const t of tasks) {
      if (!this.tasksByTile.has(t.tile_id)) {
        this.tasksByTile.set(t.tile_id, []);
      }
      this.tasksByTile.get(t.tile_id).push({ ...t });
    }
  }

  addCorridorsForTile(tileId, corridors) {
    this.corridorsByTile.set(
      tileId,
      corridors.map(c => ({ ...c }))
    );
  }

  addGaiaSnapshots(snapshots) {
    for (const s of snapshots) {
      if (!this.gaiaByTile.has(s.tile_id)) {
        this.gaiaByTile.set(s.tile_id, []);
      }
      this.gaiaByTile.get(s.tile_id).push({ ...s });
    }
  }

  getTasks(tileId, actionKind = null) {
    const tasks = this.tasksByTile.get(tileId) || [];
    if (!actionKind) return tasks.slice();
    return tasks.filter(t => t.action_kind === actionKind);
  }

  getCorridors(tileId) {
    const corridors = this.corridorsByTile.get(tileId) || [];
    return corridors.slice();
  }

  getLatestGaiaSnapshot(tileId) {
    const snaps = this.gaiaByTile.get(tileId) || [];
    if (snaps.length === 0) return null;
    return snaps.reduce((latest, snap) =>
      snap.timestamp_utc > latest.timestamp_utc ? snap : latest
    );
  }

  summarizeTile(tileId) {
    const tasks = this.getTasks(tileId, null);
    const corridors = this.getCorridors(tileId);
    const latestGaia = this.getLatestGaiaSnapshot(tileId);
    const maxTaskRoH =
      tasks.length === 0
        ? null
        : tasks.reduce(
            (max, t) => (t.r_score > max ? t.r_score : max),
            0
          );

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
 * LocalTileSpaceGraph: manages virtual tiles and safe transitions between them.
 */
export class LocalTileSpaceGraph {
  /**
   * @param {VirtualTile[]} tiles
   * @param {LocalTileObjectIndex} index
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

    this.index = index;

    if (!this.tiles.has(initialTileId)) {
      throw new Error(
        `Initial tile ${initialTileId} does not exist in LocalTileSpaceGraph.`
      );
    }

    this.activeTileId = initialTileId;
  }

  getActiveTile() {
    const tile = this.tiles.get(this.activeTileId);
    if (!tile) {
      throw new Error("Active tile missing from graph.");
    }
    return tile;
  }

  getActiveNeighbors() {
    return this.getActiveTile().neighborIds.slice();
  }

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

  summarizeActiveTile() {
    return this.index.summarizeTile(this.activeTileId);
  }

  summarizeNeighborTiles() {
    const neighborIds = this.getActiveNeighbors();
    return neighborIds.map(id => this.index.summarizeTile(id));
  }

  getActiveTileTasks(actionKind = null) {
    return this.index.getTasks(this.activeTileId, actionKind);
  }

  getActiveTileLatestGaia() {
    return this.index.getLatestGaiaSnapshot(this.activeTileId);
  }

  getActiveTileCorridors() {
    return this.index.getCorridors(this.activeTileId);
  }
}

/**
 * TileObjectIndex: facade for tile-centric objects over MCP (imagery, envelopes, eco-stations).
 * Uses EcoPlanner-aware EcoStationRegistryDuty for eco-station queries.
 */
export class TileObjectIndex {
  /**
   * @param {(request:any) => Promise<any>} transport
   */
  constructor(transport) {
    this.transport = transport;
    this.ecoStationRegistry = new EcoStationRegistryDuty(transport);
  }

  /**
   * Fetch eco-station info for a tile, gated by EcoPlanner routing.
   */
  async ecoStationByTile(options) {
    return this.ecoStationRegistry.ecoStationByTile(options);
  }
}

/**
 * TileSpaceDuty: primary browser-duty entry point for AI agents.
 * Combines MCP-backed TileObjectIndex with optional LocalTileSpaceGraph.
 */
export class TileSpaceDuty {
  /**
   * @param {(request:any) => Promise<any>} transport
   * @param {LocalTileSpaceGraph | null} localGraph
   */
  constructor(transport, localGraph = null) {
    this.transport = transport;
    this.index = new TileObjectIndex(transport);
    this.localGraph = localGraph;
  }

  async getEcoStationForTile(options) {
    return this.index.ecoStationByTile(options);
  }

  /**
   * Optional helpers that surface local graph state when present.
   */

  getActiveTileSummary() {
    if (!this.localGraph) return null;
    return this.localGraph.summarizeActiveTile();
  }

  getNeighborTileSummaries() {
    if (!this.localGraph) return [];
    return this.localGraph.summarizeNeighborTiles();
  }

  stepToNeighbor(targetTileId) {
    if (!this.localGraph) {
      throw new Error("LocalTileSpaceGraph not configured for TileSpaceDuty.");
    }
    return this.localGraph.transitionToNeighbor(targetTileId);
  }
}

export default TileSpaceDuty;
