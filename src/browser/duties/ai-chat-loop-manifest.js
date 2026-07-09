// src/browser/duties/ai-chat-loop-manifest.js
"use strict";

/**
 * Read-only manifest describing how AI-chat sessions, MCP guards,
 * EcoPlanner session state, and psych-risk bands interact.
 * This is for AI agents and diagnostics, not for actuation.
 */
export const AiChatLoopManifest = Object.freeze({
  id: "ai-chat-loop",
  version: "Phoenix-2026-v1",
  components: {
    sessionDuty: "AiChatSessionDuty",
    mcpBoundaryGuard: "MCPBoundaryGuard",
    ecoPlannerState: "EcoPlannerSessionState",
    errorType: "EcoRestorationError",
  },
  invariants: {
    // Host envelopes must satisfy RoH and lifeforce conditions before any MCP tool runs.
    hostEnvelope: {
      rohCeiling: 0.30,
      lifeforceFloor: 0.50,
      allowedLanes: ["RESEARCH", "PILOT", "PROD", "GOV_EXP_PROD"],
    },
    // Eco-planner routes are monotone in KER and corridor-safe.
    ecoPlanner: {
      usesRouteNanoswarmEnergy: true,
      monotoneLyapunov: true,
      kerWindowFamily: "CyboquaticEcosafetyEnvelopePhoenix2026v1",
    },
    // Psych-risk bands are non-carceral: they throttle missions instead of revoking access.
    psychRisk: {
      bands: ["Green", "Yellow", "Red"],
      source: "cybercore-psychrisk",
      downgradePolicy: {
        Yellow: "RESEARCH",
        Red: "SIM",
      },
    },
  },
  session: {
    featureFlag: "ai-chat-api",
    requiresDidComm: true,
    storesEcoPlannerState: true,
    fields: [
      "session_id",
      "did",
      "created_at_utc",
      "last_seen_utc",
      "eco_state.last_route_id",
      "eco_state.last_mission_class",
      "eco_state.last_m_eco",
      "eco_state.last_decision",
    ],
  },
  errorContract: {
    type: "EcoRestorationError",
    fields: [
      "kind",
      "message",
      "psych_risk.band",
      "psych_risk.neurofeedback_risk",
      "psych_risk.suggested_mission_downgrade",
    ],
  },
  mcpTools: {
    session: ["create_session", "destroy_session", "session_heartbeat"],
    domainExamples: ["fog_guard_route", "tile_space_probe", "eco_lane_planner"],
  },
});

export default AiChatLoopManifest;
