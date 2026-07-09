// src/browser/comet-ar-integration.js
/**
 * Comet Browser AR Integration Module
 * 
 * Provides augmented reality integration for superintelligence systems,
 * enabling real-time ecosafety visualization and spatial governance overlays.
 * 
 * Supports:
 * - WebXR sessions
 * - Three.js / A-Frame scenes
 * - Real-time FogGuard verdict visualization
 * - Cross-constellation route overlays
 * - Tile-space geo-anchored displays
 */

import { FogRouteAROverlay } from './ar/fog-route-ar-overlay.js';
import { CometBrowserTransport } from './comet-browser.js';

/**
 * CometARScene
 * 
 * Manages 3D scene objects for AR ecosafety overlays
 */
export class CometARScene {
  constructor(scene, options = {}) {
    this.scene = scene;
    this.camera = options.camera || null;
    this.renderer = options.renderer || null;
    this.nodeObjects = new Map();
    this.overlayGroups = new Map();
    this.animationFrame = null;
  }

  /**
   * Create or retrieve 3D object for a node
   * @param {string} nodeId - Node identifier
   * @param {object} position - 3D position {x, y, z}
   * @returns {object} Three.js Mesh or equivalent
   */
  getOrCreateNodeObject(nodeId, position = { x: 0, y: 0, z: 0 }) {
    if (this.nodeObjects.has(nodeId)) {
      return this.nodeObjects.get(nodeId);
    }

    // Create default marker (sphere with emissive material)
    const geometry = new THREE.SphereGeometry(0.5, 32, 32);
    const material = new THREE.MeshStandardMaterial({
      color: 0x888888,
      emissive: 0x222222,
      metalness: 0.5,
      roughness: 0.5
    });

    const mesh = new THREE.Mesh(geometry, material);
    mesh.position.set(position.x, position.y, position.z);
    mesh.userData.nodeId = nodeId;

    this.scene.add(mesh);
    this.nodeObjects.set(nodeId, mesh);

    return mesh;
  }

  /**
   * Update node object visual state based on ecosafety verdict
   * @param {string} nodeId - Node identifier
   * @param {object} verdict - FogGuard verdict
   * @param {object} envelope - Ecosafety envelope
   */
  updateNodeVisuals(nodeId, verdict, envelope) {
    const nodeObject = this.nodeObjects.get(nodeId);
    if (!nodeObject) return;

    // Color by verdict
    const isAllowed = verdict && verdict.verdict === 'Allow';
    const targetColor = isAllowed ? 0x00ff00 : 0xff0000;

    // Smooth color transition
    nodeObject.material.color.setHex(targetColor);
    nodeObject.material.emissive.setHex(isAllowed ? 0x004400 : 0x440000);

    // Attach label with KER data
    if (envelope && envelope.ker) {
      nodeObject.userData.labelData = {
        k: envelope.ker.k,
        e: envelope.ker.e,
        r: envelope.ker.r,
        roh: envelope.roh,
        verdict: verdict?.verdict || 'Unknown'
      };
    }
  }

  /**
   * Create AR overlay group for cross-constellation visualization
   * @param {string} groupId - Overlay group identifier
   * @returns {object} Three.js Group
   */
  createOverlayGroup(groupId) {
    if (this.overlayGroups.has(groupId)) {
      return this.overlayGroups.get(groupId);
    }

    const group = new THREE.Group();
    group.userData.groupId = groupId;
    this.scene.add(group);
    this.overlayGroups.set(groupId, group);

    return group;
  }

  /**
   * Render cross-constellation gate status
   * @param {string} nodeId - Node identifier
   * @param {string} gateStatus - Gate status
   * @param {string} fogRouteDecision - Route decision
   * @param {object} diagnostics - Gate diagnostics
   */
  renderCrossConstellationGate(nodeId, gateStatus, fogRouteDecision, diagnostics) {
    const nodeObject = this.nodeObjects.get(nodeId);
    if (!nodeObject) return;

    const isAccepted = gateStatus === 'Accepted' && fogRouteDecision === 'AllowRoute';
    const gateColor = isAccepted ? 0x00ffff : 0xff00ff;

    // Create ring indicator around node
    const ringGeometry = new THREE.TorusGeometry(0.8, 0.05, 16, 64);
    const ringMaterial = new THREE.MeshBasicMaterial({
      color: gateColor,
      transparent: true,
      opacity: 0.7
    });

    const ring = new THREE.Mesh(ringGeometry, ringMaterial);
    ring.rotation.x = Math.PI / 2;
    ring.position.copy(nodeObject.position);
    ring.position.y += 0.6;

    ring.userData.gateInfo = {
      nodeId,
      gateStatus,
      decision: fogRouteDecision,
      rW_norm: diagnostics?.rW_norm
    };

    // Remove existing ring if present
    const existingRing = this.scene.children.find(
      c => c.userData.gateInfo?.nodeId === nodeId
    );
    if (existingRing) {
      this.scene.remove(existingRing);
    }

    this.scene.add(ring);
  }

  /**
   * Create floating label for node
   * @param {string} nodeId - Node identifier
   * @param {object} data - Label data
   * @returns {object} Label sprite
   */
  createFloatingLabel(nodeId, data) {
    const canvas = document.createElement('canvas');
    canvas.width = 512;
    canvas.height = 256;
    const ctx = canvas.getContext('2d');

    // Background
    ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
    ctx.fillRect(0, 0, 512, 256);

    // Text
    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 24px monospace';
    ctx.fillText(`Node: ${nodeId}`, 20, 40);

    ctx.font = '18px monospace';
    let y = 80;
    for (const [key, value] of Object.entries(data)) {
      ctx.fillText(`${key}: ${value}`, 20, y);
      y += 30;
    }

    const texture = new THREE.CanvasTexture(canvas);
    const material = new THREE.SpriteMaterial({ map: texture });
    const sprite = new THREE.Sprite(material);
    sprite.scale.set(2, 1, 1);

    return sprite;
  }

  /**
   * Update all floating labels
   */
  updateLabels() {
    for (const [nodeId, mesh] of this.nodeObjects) {
      const labelData = mesh.userData.labelData;
      if (!labelData) continue;

      // Remove old label
      const existingLabel = this.scene.children.find(
        c => c.userData.labelFor === nodeId
      );
      if (existingLabel) {
        this.scene.remove(existingLabel);
      }

      // Create new label positioned above node
      const label = this.createFloatingLabel(nodeId, labelData);
      label.position.copy(mesh.position);
      label.position.y += 1.5;
      label.userData.labelFor = nodeId;

      this.scene.add(label);
    }
  }

  /**
   * Clear all AR objects
   */
  clear() {
    for (const mesh of this.nodeObjects.values()) {
      this.scene.remove(mesh);
      mesh.geometry?.dispose();
      mesh.material?.dispose();
    }
    this.nodeObjects.clear();

    for (const group of this.overlayGroups.values()) {
      this.scene.remove(group);
    }
    this.overlayGroups.clear();
  }

  /**
   * Cleanup resources
   */
  destroy() {
    this.clear();
    if (this.animationFrame) {
      cancelAnimationFrame(this.animationFrame);
    }
  }
}

/**
 * CometARSession
 * 
 * Manages WebXR session with Comet Browser integration
 */
export class CometARSession {
  constructor(options = {}) {
    this.xrSession = null;
    this.arScene = null;
    this.arOverlay = null;
    this.transport = null;
    this.frameId = null;
    this.isImmersive = false;
  }

  /**
   * Initialize AR session with WebXR
   * @param {object} options - Initialization options
   * @returns {Promise<boolean>} Success status
   */
  async initialize(options = {}) {
    const {
      scene,
      camera,
      renderer,
      endpoint = '/api/mcp-gateway',
      immersive = true
    } = options;

    if (!scene || !camera || !renderer) {
      throw new Error('AR session requires scene, camera, and renderer');
    }

    this.transport = new CometBrowserTransport({ endpoint });
    this.arScene = new CometARScene(scene, { camera, renderer });

    if (immersive) {
      return await this.requestImmersiveSession();
    } else {
      return await this.startInlineAR();
    }
  }

  /**
   * Request immersive AR session via WebXR
   * @returns {Promise<boolean>}
   */
  async requestImmersiveSession() {
    if (!navigator.xr) {
      console.warn('[CometAR] WebXR not available, falling back to inline AR');
      return await this.startInlineAR();
    }

    try {
      const supported = await navigator.xr.isSessionSupported('immersive-ar');
      if (!supported) {
        console.warn('[CometAR] Immersive AR not supported');
        return await this.startInlineAR();
      }

      this.xrSession = await navigator.xr.requestSession('immersive-ar', {
        requiredFeatures: ['hit-test', 'dom-overlay'],
        optionalFeatures: ['local-floor', 'bounded-floor', 'hand-tracking']
      });

      this.isImmersive = true;
      this.setupXRSession();
      return true;
    } catch (error) {
      console.error('[CometAR] Failed to start immersive session:', error);
      return await this.startInlineAR();
    }
  }

  /**
   * Start inline AR mode (non-immersive)
   * @returns {Promise<boolean>}
   */
  async startInlineAR() {
    this.isImmersive = false;
    this.xrSession = { requestAnimationFrame: (cb) => requestAnimationFrame(cb) };
    this.setupXRSession();
    return true;
  }

  /**
   * Setup XR session callbacks and rendering loop
   */
  setupXRSession() {
    if (!this.xrSession) return;

    const gl = this.arScene.renderer.context;
    const baseLayer = new XRWebGLLayer(this.xrSession, gl);
    this.xrSession.updateRenderState({ baseLayer });

    this.xrSession.addEventListener('select', (event) => {
      this.handleSelect(event);
    });

    this.xrSession.requestAnimationFrame((timestamp, frame) => {
      this.renderFrame(timestamp, frame);
    });
  }

  /**
   * Handle XR select event (user interaction)
   * @param {XRInputSourceEvent} event
   */
  handleSelect(event) {
    console.log('[CometAR] Select event:', event);
    // Could trigger node selection, info display, etc.
  }

  /**
   * Render frame callback
   * @param {number} timestamp
   * @param {XRFrame} frame
   */
  renderFrame(timestamp, frame) {
    if (!this.xrSession) return;

    const gl = this.arScene.renderer.context;
    const baseLayer = this.xrSession.renderState.baseLayer;

    this.xrSession.requestAnimationFrame((ts, fr) => this.renderFrame(ts, fr));

    if (this.isImmersive && frame) {
      const pose = frame.getViewerPose(baseLayer.getReferenceSpace());
      if (pose) {
        this.arScene.camera.projectionMatrix.fromArray(pose.views[0].projectionMatrix);
      }
    }

    this.arScene.updateLabels();
    this.arScene.renderer.render(this.arScene.scene, this.arScene.camera);
  }

  /**
   * Attach FogRouteAROverlay for ecosafety visualization
   * @param {CometBrowserSession} browserSession
   */
  attachBrowserSession(browserSession) {
    if (!this.arScene) {
      throw new Error('AR session not initialized');
    }

    const dutyTransport = browserSession.transport.asDutyTransport();
    this.arOverlay = new FogRouteAROverlay(
      dutyTransport,
      this.xrSession,
      this.arScene.scene
    );

    // Override getOrCreateNodeObject to use our scene manager
    this.arOverlay.getOrCreateNodeObject = (nodeId) => {
      return this.arScene.getOrCreateNodeObject(nodeId);
    };

    console.log('[CometAR] Browser session attached');
  }

  /**
   * Display node with ecosafety overlay
   * @param {string} nodeId - Node identifier
   * @param {object} options - Display options
   */
  async displayNode(nodeId, options = {}) {
    if (!this.arOverlay) {
      throw new Error('No browser session attached');
    }

    await this.arOverlay.updateLocalNodeOverlay(nodeId, options);
  }

  /**
   * Display cross-constellation route
   * @param {object} params - Route parameters
   */
  async displayCrossRoute(params) {
    if (!this.arOverlay) {
      throw new Error('No browser session attached');
    }

    await this.arOverlay.updateCrossConstellationOverlay(params);
  }

  /**
   * End AR session
   */
  async endSession() {
    if (this.xrSession) {
      await this.xrSession.end();
      this.xrSession = null;
    }

    if (this.arScene) {
      this.arScene.destroy();
      this.arScene = null;
    }

    this.arOverlay = null;
    this.isImmersive = false;
    console.log('[CometAR] Session ended');
  }
}

/**
 * CometARExporter
 * 
 * Export utilities for AR session data and screenshots
 */
export class CometARExporter {
  /**
   * Capture current AR scene as image
   * @param {CometARScene} arScene
   * @returns {string} Data URL
   */
  static captureScreenshot(arScene) {
    if (!arScene?.renderer) {
      throw new Error('Invalid AR scene');
    }

    const canvas = arScene.renderer.domElement;
    return canvas.toDataURL('image/png');
  }

  /**
   * Export session state as JSON
   * @param {CometARSession} arSession
   * @returns {object} Session state
   */
  static exportSessionState(arSession) {
    const state = {
      timestamp: Date.now(),
      isImmersive: arSession.isImmersive,
      nodes: []
    };

    if (arSession.arScene) {
      for (const [nodeId, mesh] of arSession.arScene.nodeObjects) {
        state.nodes.push({
          nodeId,
          position: mesh.position.toArray(),
          userData: mesh.userData
        });
      }
    }

    return state;
  }

  /**
   * Download session state as file
   * @param {object} state - Session state
   * @param {string} filename - Output filename
   */
  static downloadState(state, filename = 'comet-ar-session.json') {
    const blob = new Blob([JSON.stringify(state, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  }
}

/**
 * Factory function for creating AR sessions
 * @param {object} options - Session options
 * @returns {CometARSession}
 */
export function createARSession(options = {}) {
  return new CometARSession(options);
}

export default {
  CometARScene,
  CometARSession,
  CometARExporter,
  createARSession
};
