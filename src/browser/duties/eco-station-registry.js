// src/browser/duties/eco-station-registry.js

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
    const R = 6371; // Earth radius in km
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

    // Closest first
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
