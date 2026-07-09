// src/browser/comet-mcp-adapter.js
/**
 * Comet Browser MCP (Model Context Protocol) Adapter
 * 
 * Provides standardized MCP tool definitions for AI agent integration
 * with the Comet Browser framework. Enables superintelligence systems
 * to interact with ecosafety-governed navigation and AR overlays.
 */

import { CometBrowserSession, CometBrowserTransport } from './comet-browser.js';

/**
 * MCP Tool Definitions for Comet Browser capabilities
 */
export const CometMCPTools = {
  /**
   * Navigate to an ecosafety-governed node
   */
  navigate_to_node: {
    description: 'Navigate to a node with ecosafety verification and FogGuard approval',
    inputSchema: {
      type: 'object',
      properties: {
        nodeId: {
          type: 'string',
          description: 'Target node identifier'
        },
        corridorPresent: {
          type: 'boolean',
          description: 'Whether corridor constraints apply',
          default: true
        },
        familyId: {
          type: 'string',
          description: 'Ecosafety envelope family identifier'
        }
      },
      required: ['nodeId']
    }
  },

  /**
   * Query ecosafety envelopes
   */
  query_envelopes: {
    description: 'Query ecosafety envelopes for multiple nodes',
    inputSchema: {
      type: 'object',
      properties: {
        nodeIds: {
          type: 'array',
          items: { type: 'string' },
          description: 'List of node identifiers to query'
        },
        limit: {
          type: 'integer',
          description: 'Maximum envelopes per node',
          default: 1
        },
        familyId: {
          type: 'string',
          description: 'Envelope family filter'
        }
      },
      required: ['nodeIds']
    }
  },

  /**
   * Get KER window summary
   */
  get_ker_window: {
    description: 'Get K-E-R (Knowledge-Evidence-Residual) window summary for a node',
    inputSchema: {
      type: 'object',
      properties: {
        nodeId: {
          type: 'string',
          description: 'Node identifier'
        }
      },
      required: ['nodeId']
    }
  },

  /**
   * Evaluate cross-constellation route
   */
  evaluate_cross_constellation_route: {
    description: 'Evaluate cross-constellation FOG route for AI agent workflows',
    inputSchema: {
      type: 'object',
      properties: {
        nodeId: {
          type: 'string',
          description: 'Node identifier'
        },
        originConstellation: {
          type: 'string',
          description: 'Source constellation identifier'
        },
        targetConstellation: {
          type: 'string',
          description: 'Target constellation identifier'
        },
        workflowId: {
          type: 'string',
          description: 'Workflow identifier'
        },
        familyId: {
          type: 'string',
          description: 'Envelope family identifier'
        },
        windowId: {
          type: 'string',
          description: 'Optional window identifier'
        }
      },
      required: ['nodeId', 'originConstellation', 'targetConstellation', 'workflowId']
    }
  },

  /**
   * Query tile space
   */
  query_tile_space: {
    description: 'Query spatial tile context for AR and geo-aware navigation',
    inputSchema: {
      type: 'object',
      properties: {
        lat: {
          type: 'number',
          description: 'Latitude center point'
        },
        lon: {
          type: 'number',
          description: 'Longitude center point'
        },
        radius: {
          type: 'number',
          description: 'Search radius in kilometers',
          default: 5
        }
      },
      required: ['lat', 'lon']
    }
  },

  /**
   * Check tab health
   */
  check_tab_health: {
    description: 'Monitor tab health for AI agent dashboard interfaces',
    inputSchema: {
      type: 'object',
      properties: {
        tabId: {
          type: 'string',
          description: 'Tab identifier'
        }
      },
      required: ['tabId']
    }
  },

  /**
   * Get session summary
   */
  get_session_summary: {
    description: 'Get current browser session state summary for agent reporting',
    inputSchema: {
      type: 'object',
      properties: {}
    }
  },

  /**
   * Initialize governance context
   */
  initialize_governance: {
    description: 'Initialize ALN governance shard for session',
    inputSchema: {
      type: 'object',
      properties: {
        alnUrl: {
          type: 'string',
          description: 'URL to ALN governance shard file'
        }
      },
      required: ['alnUrl']
    }
  }
};

/**
 * CometMCPAdapter
 * 
 * Bridges Comet Browser sessions with MCP-compatible AI agents
 */
export class CometMCPAdapter {
  constructor(session, options = {}) {
    this.session = session;
    this.endpoint = options.endpoint || '/api/mcp-gateway';
    this.toolPrefix = options.toolPrefix || 'comet_';
  }

  /**
   * Generate MCP tool handlers from Comet Browser session
   * @returns {object} MCP tools with handlers
   */
  createTools() {
    return {
      [`${this.toolPrefix}navigate_to_node`]: async (args) => {
        const { nodeId, corridorPresent = true, familyId } = args;
        return await this.session.navigateToNode(nodeId, { corridorPresent, familyId });
      },

      [`${this.toolPrefix}query_envelopes`]: async (args) => {
        const { nodeIds, limit = 1, familyId } = args;
        const result = await this.session.queryEnvelopes(nodeIds, { limit, familyId });
        return Object.fromEntries(result);
      },

      [`${this.toolPrefix}get_ker_window`]: async (args) => {
        const { nodeId } = args;
        return await this.session.getKerWindow(nodeId);
      },

      [`${this.toolPrefix}evaluate_cross_constellation_route`]: async (args) => {
        return await this.session.evaluateCrossConstellationRoute(args);
      },

      [`${this.toolPrefix}query_tile_space`]: async (args) => {
        const { lat, lon, radius = 5 } = args;
        return await this.session.queryTileSpace({ lat, lon, radius });
      },

      [`${this.toolPrefix}check_tab_health`]: async (args) => {
        const { tabId } = args;
        return await this.session.checkTabHealth(tabId);
      },

      [`${this.toolPrefix}get_session_summary`]: async () => {
        return this.session.getSummary();
      },

      [`${this.toolPrefix}initialize_governance`]: async (args) => {
        const { alnUrl } = args;
        return await this.session.initializeGovernance(alnUrl);
      }
    };
  }

  /**
   * Generate MCP server configuration
   * @returns {object} MCP server config
   */
  getServerConfig() {
    const tools = this.createTools();
    const toolDefinitions = {};

    for (const [toolName, handler] of Object.entries(tools)) {
      const baseName = toolName.replace(this.toolPrefix, '');
      if (CometMCPTools[baseName]) {
        toolDefinitions[toolName] = {
          ...CometMCPTools[baseName],
          handler
        };
      }
    }

    return {
      name: 'comet-browser',
      version: '1.0.0',
      description: 'AI browser for ecosafety-governed navigation with AR support',
      tools: toolDefinitions
    };
  }

  /**
   * Handle MCP request
   * @param {object} request - MCP request object
   * @returns {Promise<object>} MCP response
   */
  async handleRequest(request) {
    const { method, params } = request;

    if (!method.startsWith(this.toolPrefix)) {
      throw new Error(`Unknown MCP method: ${method}`);
    }

    const tools = this.createTools();
    const handler = tools[method];

    if (!handler) {
      throw new Error(`MCP tool not found: ${method}`);
    }

    try {
      const result = await handler(params || {});
      return {
        jsonrpc: '2.0',
        result,
        id: request.id
      };
    } catch (error) {
      return {
        jsonrpc: '2.0',
        error: {
          code: -32000,
          message: error.message
        },
        id: request.id
      };
    }
  }
}

/**
 * Create MCP adapter from existing session
 * @param {CometBrowserSession} session
 * @param {object} options
 * @returns {CometMCPAdapter}
 */
export function createMCPAdapter(session, options = {}) {
  return new CometMCPAdapter(session, options);
}

/**
 * Create MCP-enabled AI session directly
 * @param {object} config - Session configuration
 * @param {object} mcpOptions - MCP adapter options
 * @returns {Promise<{session: CometBrowserSession, adapter: CometMCPAdapter}>}
 */
export async function createMCPEnabledSession(config, mcpOptions = {}) {
  const { CometBrowserFactory } = await import('./comet-browser.js');
  
  const session = await CometBrowserFactory.createAISession(config);
  const adapter = new CometMCPAdapter(session, mcpOptions);

  return { session, adapter };
}

export default {
  CometMCPTools,
  CometMCPAdapter,
  createMCPAdapter,
  createMCPEnabledSession
};
