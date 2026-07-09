// src/browser/duties/ppx-continuity-eco-mcp-duty.js
// Non-actuating MCP → EcoPlanner gate for AI-native browsers.

"use strict";

/**
 * EcoRouteDescriptor declares what a given MCP tool needs from the eco-planner.
 * This mirrors the Rust EcoPlanner trait contracts without inventing new semantics.
 */
class EcoRouteDescriptor {
  constructor({
    toolId,
    requiredCorridors,
    requiredPlanes,
    minKER,
    routeClass,
  }) {
    this.toolId = toolId; // e.g. "fog_guard_route", "tile_space_probe"
    this.requiredCorridors = requiredCorridors; // e.g. ["FOG", "TILE", "NANOSWARM"]
    this.requiredPlanes = requiredPlanes; // e.g. ["r_fog", "r_child", "r_roh"]
    this.minKER = minKER; // { kMin: number, eMin: number, rMax: number }
    this.routeClass = routeClass; // e.g. "diagnostic", "planning", "actuation-gated"
  }
}

/**
 * MCPRoutePlannerGate is the browser-duty mirror of Rust's EcoPlanner gate:
 * it decides whether an MCP call may proceed, based on RiskVector + KER + route nanoswarm energy.
 */
export class MCPRoutePlannerGate {
  constructor(options) {
    this.routes = new Map();
    this.defaultKER = options?.defaultKER ?? { kMin: 0.0, eMin: 0.0, rMax: 1.0 };
  }

  /**
   * Register an MCP tool → eco route descriptor mapping.
   */
  registerRoute(descriptor) {
    if (!(descriptor instanceof EcoRouteDescriptor)) {
      throw new Error("registerRoute expects EcoRouteDescriptor");
    }
    this.routes.set(descriptor.toolId, descriptor);
  }

  /**
   * Compute a routenanoswarmenergy pre-check field from RiskVector + KERWindow.
   * This stays JS-side and mirrors the Rust computation used inside EcoPlanner.
   */
  computeRouteNanoEnergy({ riskVector, kerWindow }) {
    const k = kerWindow?.k ?? 0.0;
    const e = kerWindow?.e ?? 0.0;
    const r = kerWindow?.r ?? 0.0;
    const residual = riskVector?.lyapunovResidual ?? 0.0;
    return {
      k,
      e,
      r,
      residual,
      // simple scalar summarizing eco-wealth under this route, non-negative by design
      ecoWealthScore: Math.max(0, k + e - r),
    };
  }

  /**
   * Gate an MCP request before it hits Rust MCP handlers.
   *
   * Inputs:
   * - toolId: string – MCP tool identifier
   * - mcpMessage: { riskVector, kerWindow, routenanoswarmenergy? }
   *
   * Output:
   * - { allowed: boolean, reason?: string, updatedMessage?: object }
   *
   * Reasons are aligned with Rust-side Reject("missing_data") semantics.
   */
  gate(toolId, mcpMessage) {
    const route = this.routes.get(toolId);
    if (!route) {
      return {
        allowed: false,
        reason: "missing_route_descriptor",
      };
    }

    const riskVector = mcpMessage?.riskVector ?? null;
    const kerWindow = mcpMessage?.kerWindow ?? null;

    if (!riskVector || !kerWindow) {
      return {
        allowed: false,
        reason: "missing_data",
      };
    }

    const rn = this.computeRouteNanoEnergy({ riskVector, kerWindow });

    const kOk = rn.k >= route.minKER.kMin;
    const eOk = rn.e >= route.minKER.eMin;
    const rOk = rn.r <= route.minKER.rMax;

    if (!kOk || !eOk || !rOk) {
      return {
        allowed: false,
        reason: "missing_data",
      };
    }

    const rnResidualOk =
      typeof rn.residual === "number" && Number.isFinite(rn.residual);
    if (!rnResidualOk) {
      return {
        allowed: false,
        reason: "missing_data",
      };
    }

    const updatedMessage = {
      ...mcpMessage,
      routenanoswarmenergy: rn,
    };

    return {
      allowed: true,
      updatedMessage,
    };
  }
}

/**
 * Factory that pre-wires the eight MCP tools you described into eco routes.
 * Tool IDs map directly to Rust handler names for continuity.
 */
export function createDefaultMCPRouteGate() {
  const gate = new MCPRoutePlannerGate({});

  // Example eco route descriptors. These are aligned with your Lyapunov/KER grammar.
  const defaultKER = { kMin: 0.1, eMin: 0.1, rMax: 0.9 };

  const routes = [
    new EcoRouteDescriptor({
      toolId: "fog_guard_route",
      requiredCorridors: ["FOG", "SEWER", "ROH"],
      requiredPlanes: ["r_fog", "r_child", "r_roh"],
      minKER: { ...defaultKER, rMax: 0.3 },
      routeClass: "actuation-gated",
    }),
    new EcoRouteDescriptor({
      toolId: "tile_space_probe",
      requiredCorridors: ["TILE", "ENERGY"],
      requiredPlanes: ["r_tile", "r_heat"],
      minKER: { ...defaultKER, rMax: 0.5 },
      routeClass: "diagnostic",
    }),
    // Fill in remaining six tools similarly, respecting their corridor planes.
    new EcoRouteDescriptor({
      toolId: "eco_lane_planner",
      requiredCorridors: ["ECO", "KER"],
      requiredPlanes: ["r_eco", "r_pow"],
      minKER: defaultKER,
      routeClass: "planning",
    }),
    new EcoRouteDescriptor({
      toolId: "fog_tile_bridge",
      requiredCorridors: ["FOG", "TILE"],
      requiredPlanes: ["r_fog", "r_tile"],
      minKER: defaultKER,
      routeClass: "planning",
    }),
    new EcoRouteDescriptor({
      toolId: "nanoroute_probe",
      requiredCorridors: ["NANOSWARM"],
      requiredPlanes: ["r_nano"],
      minKER: defaultKER,
      routeClass: "diagnostic",
    }),
    new EcoRouteDescriptor({
      toolId: "ker_window_update",
      requiredCorridors: ["KER"],
      requiredPlanes: ["r_ker"],
      minKER: defaultKER,
      routeClass: "planning",
    }),
    new EcoRouteDescriptor({
      toolId: "ecowealth_snapshot",
      requiredCorridors: ["ECO"],
      requiredPlanes: ["r_eco"],
      minKER: defaultKER,
      routeClass: "diagnostic",
    }),
    new EcoRouteDescriptor({
      toolId: "roh_guard_probe",
      requiredCorridors: ["ROH"],
      requiredPlanes: ["r_roh"],
      minKER: { ...defaultKER, rMax: 0.3 },
      routeClass: "diagnostic",
    }),
  ];

  for (const route of routes) {
    gate.registerRoute(route);
  }

  return gate;
}
