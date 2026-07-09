// src/browser/comet-browser.js
/**
 * Comet Browser Core Module
 * 
 * A modular browser framework designed for:
 * - AI-browser compatibility
 * - Superintelligence integrations
 * - Augmented reality overlays
 * - Ecosafety governance enforcement
 * 
 * This module provides the foundation for intelligent agent navigation
 * through ecosafety-governed digital and physical spaces.
 */

import { FogRouteAROverlay } from './ar/fog-route-ar-overlay.js';
import { FogRouterBoundaryDuty } from './duties/fog-router-boundary-duty.js';
import { CyboquaticEcosafetyEnvelopeDuty } from './duties/cyboquatic-ecosafety-envelope-duty.js';
import { FogGuardQueryDuty } from './duties/fog-guard-query-duty.js';
import { CrossConstellationFogRouteDuty } from './duties/cross-constellation-fog-route-duty.js';
import { TileSpaceDuty, TileObjectIndex } from './duties/tile-space-duty.js';
import { TabHealthDuty } from './duties/tab-health-duty.js';
import { loadSchoolNanoGovernor } from './school-nano-loader.js';

/**
 * CometBrowserTransport
 * 
 * Abstract transport layer for communication between browser modules
 * and backend services (Rust/MCP gateways, SQLite, ALN shards).
 * 
 * Implementers must provide a callable interface that accepts request
 * objects and returns Promise<response>.
 */
export class CometBrowserTransport {
  constructor(options = {}) {
    this.endpoint = options.endpoint || '/api/mcp-gateway';
    this.timeout = options.timeout || 30000;
    this.headers = {
      'Content-Type': 'application/json',
      'Accept': 'application/json',
      ...options.headers
    };
  }

  /**
   * Send a request to the backend gateway
   * @param {object} request - Structured request object
   * @returns {Promise<object>} Response from backend
   */
  async send(request) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), this.timeout);

    try {
      const response = await fetch(this.endpoint, {
        method: 'POST',
        headers: this.headers,
        body: JSON.stringify(request),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        throw new Error(`Transport error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      clearTimeout(timeoutId);
      if (error.name === 'AbortError') {
        throw new Error(`Request timeout after ${this.timeout}ms`);
      }
      throw error;
    }
  }

  /**
   * Create a duty-bound transport wrapper
   * @returns {(request: any) => Promise<any>}
   */
  asDutyTransport() {
    return (request) => this.send(request);
  }
}

/**
 * CometBrowserSession
 * 
 * Represents an active browser session with AR capabilities,
 * ecosafety envelope tracking, and AI agent coordination.
 */
export class CometBrowserSession {
  constructor(options = {}) {
    this.sessionId = options.sessionId || crypto.randomUUID();
    this.transport = options.transport || new CometBrowserTransport();
    this.xrSession = options.xrSession || null;
    this.scene = options.scene || null;
    this.agentId = options.agentId || null;
    
    // Initialize duties
    const dutyTransport = this.transport.asDutyTransport();
    this.boundaryDuty = new FogRouterBoundaryDuty(dutyTransport);
    this.ecosafetyDuty = new CyboquaticEcosafetyEnvelopeDuty(dutyTransport);
    this.fogGuardDuty = new FogGuardQueryDuty(dutyTransport);
    this.crossConstellationDuty = new CrossConstellationFogRouteDuty(dutyTransport);
    this.tileSpaceDuty = new TileSpaceDuty(dutyTransport);
    this.tabHealthDuty = new TabHealthDuty(dutyTransport);

    // AR overlay manager
    this.arOverlay = null;
    if (this.xrSession && this.scene) {
      this.arOverlay = new FogRouteAROverlay(
        dutyTransport,
        this.xrSession,
        this.scene
      );
    }

    // Session state
    this.activeNodes = new Map();
    this.governanceContext = null;
  }

  /**
   * Initialize the browser session with ALN governance shard
   * @param {string} alnUrl - URL to TexasArizonaSchoolNanoCorridor ALN shard
   */
  async initializeGovernance(alnUrl) {
    try {
      this.governanceContext = await loadSchoolNanoGovernor(alnUrl);
      console.log(`[CometBrowser] Governance initialized for session ${this.sessionId}`);
      return true;
    } catch (error) {
      console.error('[CometBrowser] Failed to initialize governance:', error);
      return false;
    }
  }

  /**
   * Navigate to a node with ecosafety verification
   * @param {string} nodeId - Target node identifier
   * @param {object} options - Navigation options
   * @returns {Promise<object>} Navigation result with verdict
   */
  async navigateToNode(nodeId, options = {}) {
    const { corridorPresent = true, familyId } = options;

    // Get ecosafety envelope and fog guard verdict
    const { envelope, verdict, error } = 
      await this.boundaryDuty.getEnvelopeAndVerdict({
        nodeId,
        corridorPresent,
        familyId
      });

    if (error) {
      return { success: false, error, nodeId };
    }

    if (!verdict || verdict.verdict !== 'Allow') {
      return {
        success: false,
        nodeId,
        reason: 'Navigation blocked by FogGuard',
        verdict,
        envelope
      };
    }

    // Track active node
    this.activeNodes.set(nodeId, {
      envelope,
      verdict,
      timestamp: Date.now()
    });

    // Update AR overlay if available
    if (this.arOverlay) {
      await this.arOverlay.updateLocalNodeOverlay(nodeId, options);
    }

    return {
      success: true,
      nodeId,
      envelope,
      verdict,
      timestamp: Date.now()
    };
  }

  /**
   * Evaluate cross-constellation route for AI agent workflows
   * @param {object} params - Route evaluation parameters
   * @returns {Promise<object>} Route decision with gate diagnostics
   */
  async evaluateCrossConstellationRoute(params) {
    const response = await this.crossConstellationDuty.evaluate(params);
    
    const { nodeId, gateStatus, fogRouteDecision, gateDiagnostics } = response;

    // Update AR overlay for cross-constellation visualization
    if (this.arOverlay) {
      this.arOverlay.renderCrossOverlay(
        nodeId,
        gateStatus,
        fogRouteDecision,
        gateDiagnostics
      );
    }

    return response;
  }

  /**
   * Query ecosafety envelopes for multiple nodes
   * @param {string[]} nodeIds - List of node identifiers
   * @param {object} options - Query options
   * @returns {Promise<Map<string, object>>} Map of nodeId to envelope
   */
  async queryEnvelopes(nodeIds, options = {}) {
    const results = new Map();
    
    for (const nodeId of nodeIds) {
      try {
        const envelopes = await this.ecosafetyDuty.getEnvelopes({
          nodeId,
          limit: options.limit || 1,
          familyId: options.familyId
        });
        
        if (envelopes && envelopes.length > 0) {
          results.set(nodeId, envelopes[0]);
        }
      } catch (error) {
        console.warn(`[CometBrowser] Failed to query envelope for ${nodeId}:`, error);
      }
    }

    return results;
  }

  /**
   * Get KER window summary for a node
   * @param {string} nodeId - Node identifier
   * @returns {Promise<object>} KER window data
   */
  async getKerWindow(nodeId) {
    return this.ecosafetyDuty.getKerWindow({ nodeId });
  }

  /**
   * Monitor tab health for AI agent interfaces
   * @param {string} tabId - Tab identifier
   * @returns {Promise<object>} Tab health status
   */
  async checkTabHealth(tabId) {
    return this.tabHealthDuty.checkHealth({ tabId });
  }

  /**
   * Query tile space for spatial context
   * @param {object} bounds - Spatial bounds {lat, lon, radius}
   * @returns {Promise<object[]>} Array of tiles in bounds
   */
  async queryTileSpace(bounds) {
    // Delegate to tileSpaceDuty which handles backend communication
    // The duty expects a transport-bound request interface
    return this.tileSpaceDuty.query({ bounds });
  }

  /**
   * Attach WebXR session for AR mode
   * @param {XRSession} xrSession - WebXR session
   * @param {object} scene - Three.js or A-Frame scene
   */
  attachXRSession(xrSession, scene) {
    this.xrSession = xrSession;
    this.scene = scene;
    const dutyTransport = this.transport.asDutyTransport();
    this.arOverlay = new FogRouteAROverlay(dutyTransport, xrSession, scene);
    console.log('[CometBrowser] AR mode enabled');
  }

  /**
   * Detach WebXR session
   */
  detachXRSession() {
    this.xrSession = null;
    this.scene = null;
    this.arOverlay = null;
    console.log('[CometBrowser] AR mode disabled');
  }

  /**
   * Get session summary for AI agent reporting
   * @returns {object} Session state summary
   */
  getSummary() {
    return {
      sessionId: this.sessionId,
      agentId: this.agentId,
      activeNodes: Array.from(this.activeNodes.keys()),
      arEnabled: !!this.arOverlay,
      governanceInitialized: !!this.governanceContext,
      timestamp: Date.now()
    };
  }

  /**
   * Cleanup session resources
   */
  destroy() {
    this.detachXRSession();
    this.activeNodes.clear();
    this.governanceContext = null;
    console.log(`[CometBrowser] Session ${this.sessionId} destroyed`);
  }
}

/**
 * CometBrowserFactory
 * 
 * Factory for creating configured browser sessions
 */
export class CometBrowserFactory {
  static createSession(options = {}) {
    return new CometBrowserSession(options);
  }

  static createTransport(endpoint, options = {}) {
    return new CometBrowserTransport({ endpoint, ...options });
  }

  /**
   * Create a session optimized for AI agent use
   * @param {object} config - Configuration for AI agent session
   * @returns {Promise<CometBrowserSession>}
   */
  static async createAISession(config) {
    const {
      agentId,
      alnUrl,
      endpoint = '/api/mcp-gateway',
      xrSession = null,
      scene = null
    } = config;

    const transport = new CometBrowserTransport({ endpoint });
    const session = new CometBrowserSession({
      sessionId: `ai-${agentId}-${Date.now()}`,
      agentId,
      transport,
      xrSession,
      scene
    });

    if (alnUrl) {
      await session.initializeGovernance(alnUrl);
    }

    return session;
  }

  /**
   * Create a session optimized for AR superintelligence integration
   * @param {object} config - Configuration for AR session
   * @returns {Promise<CometBrowserSession>}
   */
  static async createARSession(config) {
    const {
      xrSession,
      scene,
      alnUrl,
      endpoint = '/api/mcp-gateway'
    } = config;

    if (!xrSession || !scene) {
      throw new Error('AR session requires xrSession and scene');
    }

    const transport = new CometBrowserTransport({ endpoint });
    const session = new CometBrowserSession({
      sessionId: `ar-${crypto.randomUUID()}`,
      transport,
      xrSession,
      scene
    });

    if (alnUrl) {
      await session.initializeGovernance(alnUrl);
    }

    return session;
  }
}

/**
 * Default export for easy import
 */
export default {
  CometBrowserTransport,
  CometBrowserSession,
  CometBrowserFactory
};
