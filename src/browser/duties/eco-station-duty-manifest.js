// src/browser/duties/eco-station-duty-manifest.js
"use strict";

/**
 * Read-only manifest describing how eco-station queries are gated.
 * Can be used by AI agents to reason about invariants.
 */
export const EcoStationDutyManifest = Object.freeze({
  id: "eco-station-registry",
  version: "Phoenix-2026-v1",
  invariants: {
    ecoPlannerGate: true,
    requiresDecision: ["Accept"],
    routedBy: "organichain_eco_planner.routenanoswarmenergy",
    kerWindowFamily: "CyboquaticEcosafetyEnvelopePhoenix2026v1",
  },
  contracts: {
    ecoStationByTileRequest: {
      type: "EcoStationRegistryRequest",
      fields: ["tileId", "constellationId", "missionClass", "familyId"],
    },
    ecoStationByTileResponse: {
      fields: [
        "tileId",
        "constellationId",
        "missionClass",
        "allowed",
        "m_eco",
        "routeId",
        "lane",
        "station",
        "diagnostics",
      ],
    },
  },
});

export default EcoStationDutyManifest;
