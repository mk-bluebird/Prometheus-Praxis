// src/browser/duties/eco-station-schema.js

/**
 * Data model for Public Works ECO Stations in Phoenix.
 * Designed for safe, queryable use by browser agents and AI-chat systems.
 */

export class EcoStation {
  /**
   * @param {Object} feature - A single GeoJSON Feature.
   */
  constructor(feature) {
    if (!feature || feature.type !== "Feature") {
      throw new Error("EcoStation requires a GeoJSON Feature.");
    }

    const props = feature.properties || {};
    const geom = feature.geometry || {};

    if (geom.type !== "Point" || !Array.isArray(geom.coordinates)) {
      throw new Error("EcoStation geometry must be a Point.");
    }

    const [lon, lat] = geom.coordinates;

    this.objectId = props.OBJECTID;
    this.name = props.NAME;
    this.fullAddress = props.FULL_ADDRE;
    this.website = props.WEBSITE;
    this.description = props.DESCRIPTION;
    this.lat = lat;
    this.lon = lon;
  }

  /**
   * Plain JSON representation for AI-chat / UI use.
   */
  toJSON() {
    return {
      objectId: this.objectId,
      name: this.name,
      fullAddress: this.fullAddress,
      website: this.website,
      description: this.description,
      lat: this.lat,
      lon: this.lon,
    };
  }
}
