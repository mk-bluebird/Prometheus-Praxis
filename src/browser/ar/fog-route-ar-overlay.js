// src/browser/ar/fog-route-ar-overlay.js

import {
  FogRouterBoundaryDuty,
} from "../duties/fog-router-boundary-duty.js";

import {
  CrossConstellationFogRouteDuty,
} from "../duties/cross-constellation-fog-route-duty.js";

export class FogRouteAROverlay {
  constructor(transport, xrSession, scene) {
    this.boundaryDuty = new FogRouterBoundaryDuty(transport);
    this.crossDuty = new CrossConstellationFogRouteDuty(transport);
    this.xrSession = xrSession; // WebXR session
    this.scene = scene;         // A-Frame / Three.js scene root
  }

  async updateLocalNodeOverlay(nodeId, options = {}) {
    const { corridorPresent = true, familyId } = options;

    const { envelope, verdict } =
      await this.boundaryDuty.getEnvelopeAndVerdict({
        nodeId,
        corridorPresent,
        familyId,
        guardConfig: null,
      });

    this.renderNodeOverlay(nodeId, envelope, verdict);
  }

  async updateCrossConstellationOverlay(params) {
    const response = await this.crossDuty.evaluate(params);

    const { nodeId, gateStatus, fogRouteDecision, gateDiagnostics } = response;

    this.renderCrossOverlay(
      nodeId,
      gateStatus,
      fogRouteDecision,
      gateDiagnostics,
    );
  }

  renderNodeOverlay(nodeId, envelope, verdict) {
    // Lookup or create 3D object for nodeId in the scene.
    const nodeObject = this.getOrCreateNodeObject(nodeId);

    // Color-code by FogGuardVerdict.
    const color =
      verdict && verdict.verdict === "Allow"
        ? "#00ff00"
        : "#ff0000";

    nodeObject.material.color.set(color);

    // Optionally attach a floating label with KER and RoH.
    nodeObject.userData.label = {
      k: envelope.ker.k,
      e: envelope.ker.e,
      r: envelope.ker.r,
      roh: envelope.roh,
    };
  }

  renderCrossOverlay(nodeId, gateStatus, fogRouteDecision, gateDiagnostics) {
    const nodeObject = this.getOrCreateNodeObject(nodeId);

    // Gate status overlay: Accepted vs Rejected.
    const gateColor =
      gateStatus === "Accepted" && fogRouteDecision === "AllowRoute"
        ? "#00ffff"
        : "#ff00ff";

    nodeObject.userData.crossGate = {
      gateStatus,
      decision: fogRouteDecision,
      rW_norm: gateDiagnostics.rW_norm,
    };

    // In a WebXR/A-Frame context, you might use a small HUD element
    // bound to the node's world position to show r_W and ecoHamiltonianDelta.
  }

  getOrCreateNodeObject(nodeId) {
    // Implementation-specific: map nodeId to 3D mesh in the scene.
    // For example, use a Map from nodeId to Three.js Mesh.
    // If absent, create a default marker and insert into the scene.
    throw new Error("getOrCreateNodeObject not implemented");
  }
}
