// src/browser/duties/ppx-continuity-schema-duty.js
// Non-actuating JSON Schema + safety envelope validation for MCP tools.

"use strict";

/**
 * ToolSchemaRegistry keeps a mapping from MCP tool IDs to JSON Schemas and
 * enforces structural validation at the browser/agent boundary.
 */
export class ToolSchemaRegistry {
  constructor() {
    this.schemas = new Map();
  }

  /**
   * Register a schema for a tool.
   *
   * @param {string} toolId
   * @param {object} schema - JSON Schema Draft-07 compatible object
   */
  registerSchema(toolId, schema) {
    if (!schema || typeof schema !== "object") {
      throw new Error("registerSchema requires a JSON schema object");
    }
    this.schemas.set(toolId, schema);
  }

  /**
   * Get the JSON schema for a tool if registered.
   */
  getSchema(toolId) {
    return this.schemas.get(toolId) ?? null;
  }

  /**
   * Validate a request payload against the tool schema.
   * This implements a minimal subset of JSON Schema (type, required, properties).
   *
   * Returns { valid: boolean, errors: string[] }.
   */
  validate(toolId, payload) {
    const schema = this.schemas.get(toolId);
    if (!schema) {
      return { valid: false, errors: ["missing_schema"] };
    }

    const errors = [];

    // Type check
    if (schema.type === "object") {
      if (!payload || typeof payload !== "object" || Array.isArray(payload)) {
        errors.push("payload_not_object");
      }
    }

    // Required fields
    if (Array.isArray(schema.required)) {
      for (const req of schema.required) {
        if (!(req in payload)) {
          errors.push(`missing_field:${req}`);
        }
      }
    }

    // Simple per-property type checks
    if (schema.properties && typeof schema.properties === "object") {
      for (const [name, propSchema] of Object.entries(schema.properties)) {
        if (!(name in payload)) continue;
        const value = payload[name];
        if (propSchema.type === "string" && typeof value !== "string") {
          errors.push(`invalid_type:${name}:expected_string`);
        } else if (propSchema.type === "number" && typeof value !== "number") {
          errors.push(`invalid_type:${name}:expected_number`);
        } else if (propSchema.type === "boolean" && typeof value !== "boolean") {
          errors.push(`invalid_type:${name}:expected_boolean`);
        } else if (propSchema.type === "object" && (typeof value !== "object" || Array.isArray(value))) {
          errors.push(`invalid_type:${name}:expected_object`);
        } else if (propSchema.type === "array" && !Array.isArray(value)) {
          errors.push(`invalid_type:${name}:expected_array`);
        }
      }
    }

    return {
      valid: errors.length === 0,
      errors,
    };
  }
}

/**
 * SafetyEnvelopeGuard mirrors neurorights_safety::SafetyEnvelopeValidator behavior
 * at the browser-duty level, without performing actuation or hardware calls.
 */
export class SafetyEnvelopeGuard {
  constructor(config) {
    this.rohCeiling = config?.rohCeiling ?? 0.30;
    this.lifeforceFloor = config?.lifeforceFloor ?? 0.50;
    this.allowedLanes = config?.allowedLanes ?? ["RESEARCH", "PILOT", "PROD", "GOV_EXP_PROD"];
  }

  /**
   * Validate host envelope invariants:
   * - RoH ≤ rohCeiling
   * - lifeforce ≥ lifeforceFloor
   * - lane ∈ allowedLanes
   *
   * Returns { verdict: "Allow" | "Derate" | "Stop", reasons: string[] }.
   */
  validateEnvelope(envelope) {
    const reasons = [];
    const roh = envelope?.roh;
    const lifeforce = envelope?.lifeforce;
    const lane = envelope?.lane;

    if (!Number.isFinite(roh)) {
      reasons.push("missing_roh");
    }
    if (!Number.isFinite(lifeforce)) {
      reasons.push("missing_lifeforce");
    }
    if (!lane || typeof lane !== "string") {
      reasons.push("missing_lane");
    }

    if (reasons.length > 0) {
      return { verdict: "Stop", reasons };
    }

    if (roh > this.rohCeiling) {
      reasons.push("roh_above_ceiling");
    }
    if (lifeforce < this.lifeforceFloor) {
      reasons.push("lifeforce_below_floor");
    }
    if (!this.allowedLanes.includes(lane)) {
      reasons.push("lane_not_allowed");
    }

    if (reasons.includes("roh_above_ceiling") || reasons.includes("lifeforce_below_floor")) {
      return { verdict: "Stop", reasons };
    }

    if (reasons.includes("lane_not_allowed")) {
      return { verdict: "Derate", reasons };
    }

    return { verdict: "Allow", reasons: [] };
  }
}

/**
 * Combined MCP boundary guard:
 * - JSON Schema structural validation via ToolSchemaRegistry
 * - Host envelope invariants via SafetyEnvelopeGuard
 */
export class MCPBoundaryGuard {
  constructor({ schemaRegistry, safetyGuard }) {
    this.schemaRegistry = schemaRegistry;
    this.safetyGuard = safetyGuard;
  }

  /**
   * Gate a toolDefinition-style call.
   *
   * Inputs:
   * - toolId: string
   * - payload: object
   * - hostEnvelope: { roh, lifeforce, lane, ... }
   *
   * Returns:
   * - { allowed: boolean, reasons: string[], verdict?: "Allow" | "Derate" | "Stop" }
   */
  gate(toolId, payload, hostEnvelope) {
    const { valid, errors } = this.schemaRegistry.validate(toolId, payload);
    if (!valid) {
      return {
        allowed: false,
        reasons: errors,
        verdict: "Stop",
      };
    }

    const envVerdict = this.safetyGuard.validateEnvelope(hostEnvelope);

    if (envVerdict.verdict === "Stop") {
      return {
        allowed: false,
        reasons: envVerdict.reasons,
        verdict: "Stop",
      };
    }

    if (envVerdict.verdict === "Derate") {
      return {
        allowed: false,
        reasons: envVerdict.reasons,
        verdict: "Derate",
      };
    }

    return {
      allowed: true,
      reasons: [],
      verdict: "Allow",
    };
  }
}

/**
 * Factory that loads schemas from pre-bundled files under
 * crates/ppx_continuity_mcp/schemas/*.json (compiled from schemars).
 */
export function createDefaultMCPBoundaryGuard(schemaLoader) {
  const registry = new ToolSchemaRegistry();

  // schemaLoader(toolId) should synchronously return the parsed JSON schema
  const toolIds = [
    "fog_guard_route",
    "tile_space_probe",
    "eco_lane_planner",
    "fog_tile_bridge",
    "nanoroute_probe",
    "ker_window_update",
    "ecowealth_snapshot",
    "roh_guard_probe",
  ];

  for (const id of toolIds) {
    const schema = schemaLoader(id);
    if (schema) {
      registry.registerSchema(id, schema);
    }
  }

  const safetyGuard = new SafetyEnvelopeGuard({
    rohCeiling: 0.30,
    lifeforceFloor: 0.50,
    allowedLanes: ["RESEARCH", "PILOT", "PROD", "GOV_EXP_PROD"],
  });

  return new MCPBoundaryGuard({ schemaRegistry: registry, safetyGuard });
}
