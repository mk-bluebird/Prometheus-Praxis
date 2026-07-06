// src/browser/duties/drinking-water-duty.js

/**
 * Browser-duty helper for querying OSM drinking water points via Overpass API.
 * Uses the standard node[amenity=drinking_water]({{bbox}}); out; pattern.[web:82][web:85]
 */

const OVERPASS_ENDPOINT = "https://overpass-api.de/api/interpreter";

/**
 * Build an Overpass QL query for drinking_water nodes in a bbox.
 *
 * @param {Object} bbox - { south, west, north, east } in degrees.
 * @returns {string}
 */
export function buildDrinkingWaterQuery(bbox) {
  const { south, west, north, east } = bbox;
  // Use explicit numeric bbox instead of {{bbox}} so agents can control scope.[web:87]
  return `
[out:json][timeout:25];
node
  ["amenity"="drinking_water"]
  (${south},${west},${north},${east});
out;
`.trim();
}

/**
 * Fetch drinking water amenities from Overpass for a given bbox.
 *
 * @param {Object} bbox - { south, west, north, east }.
 * @returns {Promise<Object>} Overpass JSON response.
 */
export async function fetchDrinkingWaterAmenities(bbox) {
  const ql = buildDrinkingWaterQuery(bbox);
  const url = `${OVERPASS_ENDPOINT}?data=${encodeURIComponent(ql)}`;

  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`Overpass request failed: ${res.status} ${res.statusText}`);
  }
  const json = await res.json();

  // Return only simplified node data for AI-chat agents.
  const nodes = (json.elements || [])
    .filter((el) => el.type === "node" && el.tags && el.tags.amenity === "drinking_water")
    .map((el) => ({
      id: el.id,
      lat: el.lat,
      lon: el.lon,
      name: el.tags.name || null,
      description: el.tags.description || null,
      tags: el.tags,
    }));

  return {
    bbox,
    count: nodes.length,
    nodes,
  };
}
