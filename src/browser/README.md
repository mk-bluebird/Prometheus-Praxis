# Comet Browser Framework

## Overview

The **Comet Browser** is a modular JavaScript framework designed for AI-browser compatibility, superintelligence integrations, and augmented reality overlays with ecosafety governance enforcement. It provides the foundation for intelligent agent navigation through ecosafety-governed digital and physical spaces.

## Architecture

```
src/browser/
├── comet-browser.js          # Core browser session & transport
├── comet-mcp-adapter.js      # MCP (Model Context Protocol) adapter
├── comet-ar-integration.js   # AR/WebXR integration
├── school-nano-loader.js     # ALN governance shard loader
├── ar/
│   └── fog-route-ar-overlay.js  # AR overlay for FOG routes
├── duties/                   # Duty modules (governance queries)
│   ├── fog-router-boundary-duty.js
│   ├── fog-guard-query-duty.js
│   ├── cyboquatic-ecosafety-envelope-duty.js
│   ├── cross-constellation-fog-route-duty.js
│   ├── tile-space-duty.js
│   └── tab-health-duty.js
└── util/
    └── markdown-pilots-table.js
```

## Key Components

### CometBrowserSession

Main session class that coordinates:
- Ecosafety envelope tracking
- FogGuard verdict evaluation
- Cross-constellation routing
- AR overlay management
- Tile space queries
- Tab health monitoring

### CometBrowserTransport

Abstract transport layer for backend communication:
- HTTP/MCP gateway interface
- Timeout handling
- Request/response formatting

### CometMCPAdapter

Bridges Comet Browser with AI agents via Model Context Protocol:
- Standardized tool definitions
- JSON-RPC request handling
- Session management

### CometARSession

WebXR integration for augmented reality:
- Immersive and inline AR modes
- Three.js scene management
- Real-time ecosafety visualization
- Floating labels with KER data

## Usage Examples

### Basic AI Agent Session

```javascript
import { CometBrowserFactory } from './comet-browser.js';

// Create AI-optimized session
const session = await CometBrowserFactory.createAISession({
  agentId: 'agent-001',
  alnUrl: '/governance/TexasArizonaSchoolNanoCorridor.aln',
  endpoint: '/api/mcp-gateway'
});

// Navigate with ecosafety verification
const result = await session.navigateToNode('node-xyz', {
  corridorPresent: true,
  familyId: 'CyboquaticEcosafetyEnvelopePhoenix2026v1'
});

if (result.success) {
  console.log('Navigation allowed:', result.verdict);
} else {
  console.log('Navigation blocked:', result.reason);
}
```

### MCP-Enabled Integration

```javascript
import { createMCPEnabledSession } from './comet-mcp-adapter.js';

const { session, adapter } = await createMCPEnabledSession({
  agentId: 'superintelligence-core',
  endpoint: '/api/mcp'
});

// Get MCP server config
const config = adapter.getServerConfig();

// Handle MCP request
const response = await adapter.handleRequest({
  jsonrpc: '2.0',
  method: 'comet_navigate_to_node',
  params: { nodeId: 'abc-123' },
  id: 1
});
```

### AR Superintelligence Integration

```javascript
import { createARSession } from './comet-ar-integration.js';
import { CometBrowserFactory } from './comet-browser.js';

// Initialize browser session
const browserSession = await CometBrowserFactory.createAISession({
  agentId: 'ar-agent',
  endpoint: '/api/mcp-gateway'
});

// Create AR session
const arSession = createARSession();
await arSession.initialize({
  scene: threeScene,
  camera: camera,
  renderer: renderer,
  immersive: true
});

// Attach browser to AR
arSession.attachBrowserSession(browserSession);

// Display node with ecosafety overlay
await arSession.displayNode('sensor-node-42', {
  corridorPresent: true
});

// Show cross-constellation route
await arSession.displayCrossRoute({
  nodeId: 'hub-001',
  originConstellation: 'texas-corridor',
  targetConstellation: 'arizona-corridor',
  workflowId: 'wf-2026-001'
});
```

### Query Ecosafety Envelopes

```javascript
// Query multiple nodes
const envelopes = await session.queryEnvelopes(
  ['node-a', 'node-b', 'node-c'],
  { limit: 1, familyId: 'CyboquaticEcosafetyEnvelopePhoenix2026v1' }
);

for (const [nodeId, envelope] of envelopes) {
  console.log(`${nodeId}: KER=${envelope.ker.k},${envelope.ker.e},${envelope.ker.r}`);
}

// Get KER window summary
const kerWindow = await session.getKerWindow('node-a');
console.log('KER deployable:', kerWindow.kerdeployable);
```

### Tile Space Queries

```javascript
// Query spatial context
const tiles = await session.queryTileSpace({
  lat: 32.7157,
  lon: -100.9686,
  radius: 10 // km
});

for (const tile of tiles) {
  console.log(`Tile ${tile.id}:`, tile.coord);
}
```

## MCP Tool Definitions

| Tool | Description |
|------|-------------|
| `comet_navigate_to_node` | Navigate with FogGuard approval |
| `comet_query_envelopes` | Query ecosafety envelopes |
| `comet_get_ker_window` | Get K-E-R window summary |
| `comet_evaluate_cross_constellation_route` | Evaluate cross-constellation routes |
| `comet_query_tile_space` | Query spatial tile context |
| `comet_check_tab_health` | Monitor dashboard tab health |
| `comet_get_session_summary` | Get session state summary |
| `comet_initialize_governance` | Initialize ALN governance shard |

## Governance Integration

Comet Browser integrates with ALN (Aletheion) governance shards:

```javascript
// Load governance context
await session.initializeGovernance(
  'https://governance.example.com/TexasArizonaSchoolNanoCorridor.aln'
);

// Check nano pilot candidates
const candidates = await session.governanceContext.nanoPilotCandidates({
  rRegulatoryMax: 0.10,
  rConsentMax: 0.30,
  rLongtermMax: 0.35
});
```

## Ecosafety Enforcement

All navigation and routing decisions respect:
- **FogGuard Verdicts**: Allow/Stop decisions based on safestep evaluation
- **Corridor Constraints**: Soil, water, habitat, heat budget limits
- **KER Windows**: Knowledge-Evidence-Residual thresholds
- **RoH Ceilings**: Residual of Harm limits

## AR Visualization

Real-time overlays display:
- **Green nodes**: FogGuard Allow verdict
- **Red nodes**: FogGuard Stop verdict
- **Cyan rings**: Accepted cross-constellation routes
- **Magenta rings**: Rejected routes
- **Floating labels**: K, E, R, RoH values

## Non-Actuating Design

Comet Browser is **purely observational**:
- No device control
- No state mutation
- Read-only governance queries
- Data-space navigation only

This ensures safety for superintelligence integrations.

## Dependencies

- WebXR API (for AR features)
- Three.js (for 3D rendering)
- Fetch API (for backend communication)
- ES Modules (native JavaScript modules)

## License

Part of the Comet Browser ecosystem for AI-safe navigation.
