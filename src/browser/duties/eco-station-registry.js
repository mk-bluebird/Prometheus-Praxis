// src/browser/duties/eco-station-registry.js

"use strict";

import { EcoStation } from "./eco-station-schema.js";

/**
 * Registry for ECO Stations, with spatial and text queries.
 * Can be backed by in-memory data or future SQLite shards.
 */
export class EcoStationRegistry {
  constructor() {
    /** @type {EcoStation[]} */
    this.stations = [];
  }

  /**
   * Load from a GeoJSON FeatureCollection.
   *
   * @param {Object} geojson
   */
  loadFromGeoJSON(geojson) {
    if (!geojson || geojson.type !== "FeatureCollection") {
      throw new Error("Expected GeoJSON FeatureCollection.");
    }

    const features = Array.isArray(geojson.features) ? geojson.features : [];
    this.stations = features.map((f) => new EcoStation(f));
  }

  /**
   * List all stations.
   *
   * @returns {Object[]} plain JSON objects
   */
  listAll() {
    return this.stations.map((s) => s.toJSON());
  }

  /**
   * Find a station by OBJECTID.
   *
   * @param {number} objectId
   * @returns {Object|null}
   */
  getByObjectId(objectId) {
    const s = this.stations.find((st) => st.objectId === objectId);
    return s ? s.toJSON() : null;
  }

  /**
   * Simple name/description search for AI-chat use.
   *
   * @param {string} query
   * @returns {Object[]}
   */
  searchByText(query) {
    if (!query || typeof query !== "string") return this.listAll();
    const q = query.toLowerCase();
    return this.stations
      .filter((s) => {
        const name = s.name?.toLowerCase() || "";
        const desc = s.description?.toLowerCase() || "";
        return name.includes(q) || desc.includes(q);
      })
      .map((s) => s.toJSON());
  }

  /**
   * Find stations within a radius of a point (simple haversine).
   *
   * @param {number} lat - degrees
   * @param {number} lon - degrees
   * @param {number} radiusKm
   * @returns {Object[]}
   */
  queryByRadius(lat, lon, radiusKm) {
    const R = 6371;
    const toRad = (deg) => (deg * Math.PI) / 180;

    const results = [];

    for (const s of this.stations) {
      const dLat = toRad(s.lat - lat);
      const dLon = toRad(s.lon - lon);
      const a =
        Math.sin(dLat / 2) * Math.sin(dLat / 2) +
        Math.cos(toRad(lat)) *
          Math.cos(toRad(s.lat)) *
          Math.sin(dLon / 2) *
          Math.sin(dLon / 2);
      const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
      const distanceKm = R * c;

      if (distanceKm <= radiusKm) {
        results.push({
          ...s.toJSON(),
          distanceKm,
        });
      }
    }

    results.sort((a, b) => a.distanceKm - b.distanceKm);
    return results;
  }
}

/**
 * Convenience helper: build a registry from a raw GeoJSON object.
 *
 * @param {Object} geojson
 * @returns {EcoStationRegistry}
 */
export function createEcoStationRegistry(geojson) {
  const registry = new EcoStationRegistry();
  registry.loadFromGeoJSON(geojson);
  return registry;
}

/**
 * Request type for EcoPlanner-gated eco-station queries.
 */
export class EcoStationRegistryRequest {
  constructor(options) {
    this.tileId = options.tileId;
    this.constellationId = options.constellationId ?? "Phoenix";
    this.missionClass = options.missionClass ?? "cyboquatic-cleanup";
    this.familyId =
      options.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
  }

  build() {
    if (!this.tileId) {
      throw new Error("EcoStationRegistryRequest requires tileId");
    }
    return {
      type: "EcoStationRegistryRequest",
      tileId: this.tileId,
      constellationId: this.constellationId,
      missionClass: this.missionClass,
      familyId: this.familyId,
    };
  }
}

/**
 * Eco-station registry duty that filters station health through EcoPlanner.
 * Non-actuating: delegates routing semantics to a Rust MCP backend.
 */
export class EcoStationRegistryDuty {
  /**
   * @param {(request:any) => Promise<any>} transport
   */
  constructor(transport) {
    this.transport = transport;
  }

  /**
   * Compute an EcoPlanner route and return station health only when mission is Accept.
   *
   * @param {Object} options
   * @returns {Promise<Object>}
   */
  async ecoStationByTile(options) {
    const req = new EcoStationRegistryRequest(options).build();

    const ecoPlannerResponse = await this.transport({
      type: "EcoPlannerRouteNanoswarmEnergyRequest",
      tileId: req.tileId,
      constellationId: req.constellationId,
      missionClass: req.missionClass,
      familyId: req.familyId,
    });

    const { decision, m_eco, routeId, lane, diagnostics } =
      ecoPlannerResponse ?? {};

    if (decision !== "Accept") {
      return {
        tileId: req.tileId,
        constellationId: req.constellationId,
        missionClass: req.missionClass,
        allowed: false,
        reason: diagnostics?.reason ?? "route_not_accepted",
        m_eco,
        routeId,
        lane,
        station: null,
      };
    }

    const stationResponse = await this.transport({
      type: "EcoStationHealthByTileRequest",
      tileId: req.tileId,
      constellationId: req.constellationId,
      routeId,
      lane,
      familyId: req.familyId,
    });

    return {
      tileId: req.tileId,
      constellationId: req.constellationId,
      missionClass: req.missionClass,
      allowed: true,
      m_eco,
      routeId,
      lane,
      station: stationResponse?.station ?? null,
      diagnostics,
    };
  }
}

export default EcoStationRegistryDuty;
