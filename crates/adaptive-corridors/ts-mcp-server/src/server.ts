// eco_restoration_shard/crates/adaptive-corridors/ts-mcp-server/src/server.ts
// Adaptive Corridors MCP server (diagnostic-only).

import {
  MCPServer,
  ToolDefinition,
  ToolHandlerContext,
} from "@modelcontextprotocol/sdk";

// Types mirroring Rust structs for Hflow guard.
type HflowLane = "Allow" | "Derate" | "Stop";

type HflowReason =
  | "RoHAboveHardCeiling"
  | "RoHOpAboveHardCeiling"
  | "TailProbAboveThreshold"
  | "WithinSoftBand"
  | "OpWithinSoftBand"
  | "WithinSafeBand"
  | "ProbabilisticCaution";

interface CapHflowPolicy {
  soft_band_start: number;
  hard_ceiling: number;
  beta: number;
  max_exceed_prob: number;
  w_vel: number;
  w_mob: number;
  w_geo: number;
}

interface CapHflowTelemetry {
  velocity_mps: number;
  turbulence_index: number;
  shear_index: number;
  shutter_open_frac: number;
  shutter_rate_per_min: number;
  mobility_index: number;
  geom_stability_index: number;
}

interface HflowProbSummary {
  mean: number;
  var: number;
  tail_prob_over_ceiling: number;
}

interface HflowGuardResult {
  corridor_id: string;
  hflow_scalar: number;
  hflow_op: number;
  r_vel: number;
  r_mob: number;
  r_geo: number;
  lane: HflowLane;
  reasons: HflowReason[];
  tail_prob_over_ceiling: number;
  safety_vector: {
    roh_ceiling: number;
    soft_band_start: number;
    hard_ceiling: number;
    max_exceed_prob: number;
    beta: number;
  };
}

// Corridor-normalization helpers (mirroring Rust).
function normVelocity(v: number): number {
  const num = v - 0.5;
  const den = 2.5 - 0.5;
  return Math.min(1.0, Math.max(0.0, num / den));
}

function normTurbulence(t: number): number {
  return Math.min(1.0, Math.max(0.0, t / 0.6));
}

function normShear(s: number): number {
  return Math.min(1.0, Math.max(0.0, s));
}

function normShutterOpen(frac: number): number {
  const num = frac - 0.2;
  const den = 1.0 - 0.2;
  return Math.min(1.0, Math.max(0.0, num / den));
}

function normShutterRate(rate: number): number {
  return Math.min(1.0, Math.max(0.0, rate / 0.1));
}

function normMobility(mu: number): number {
  return Math.min(1.0, Math.max(0.0, mu));
}

function normGeomStability(g: number): number {
  return Math.min(1.0, Math.max(0.0, g));
}

function computeSubRisks(t: CapHflowTelemetry): { r_vel: number; r_mob: number; r_geo: number } {
  const v_hat = normVelocity(t.velocity_mps);
  const turb_hat = normTurbulence(t.turbulence_index);
  const shear_hat = normShear(t.shear_index);

  const s_open_hat = normShutterOpen(t.shutter_open_frac);
  const s_rate_hat = normShutterRate(t.shutter_rate_per_min);
  const mu_hat = normMobility(t.mobility_index);

  const g_hat = normGeomStability(t.geom_stability_index);

  const r_vel = 0.4 * v_hat + 0.3 * turb_hat + 0.3 * shear_hat;
  const r_mob = 0.3 * s_open_hat + 0.3 * s_rate_hat + 0.4 * mu_hat;
  const r_geo = g_hat;

  return { r_vel, r_mob, r_geo };
}

function computeHflowScalar(policy: CapHflowPolicy, r_vel: number, r_mob: number, r_geo: number): number {
  return policy.w_vel * r_vel + policy.w_mob * r_mob + policy.w_geo * r_geo;
}

function computeHflowOp(policy: CapHflowPolicy, prob: HflowProbSummary): number {
  const std = Math.sqrt(Math.max(prob.var, 0.0));
  return prob.mean + policy.beta * std;
}

function decideLane(
  policy: CapHflowPolicy,
  h_scalar: number,
  h_op: number,
  tail_prob: number,
  reasons: HflowReason[],
): HflowLane {
  if (h_scalar >= policy.hard_ceiling) {
    reasons.push("RoHAboveHardCeiling");
    return "Stop";
  }
  if (h_op >= policy.hard_ceiling) {
    reasons.push("RoHOpAboveHardCeiling");
    return "Stop";
  }
  if (tail_prob > policy.max_exceed_prob) {
    reasons.push("TailProbAboveThreshold");
    if (h_scalar >= policy.soft_band_start) {
      return "Stop";
    }
    return "Derate";
  }

  if (h_scalar >= policy.soft_band_start) {
    reasons.push("WithinSoftBand");
    return "Derate";
  }
  if (h_op >= policy.soft_band_start) {
    reasons.push("OpWithinSoftBand");
    return "Derate";
  }

  reasons.push("WithinSafeBand");
  return "Allow";
}

function defaultCapPhxPolicy(): CapHflowPolicy {
  return {
    soft_band_start: 0.24,
    hard_ceiling: 0.30,
    beta: 2.0,
    max_exceed_prob: 0.01,
    w_vel: 0.5,
    w_mob: 0.3,
    w_geo: 0.2,
  };
}

// Tool definition aligned with ppx.function.meta.v1.
const hflowCapPhxTool: ToolDefinition = {
  name: "water.hflow_guard_cap_phx.diagnostic.v1",
  description:
    "Diagnostic-only Hflow guard for CAP Phoenix urban flood corridor. Computes lane (Allow/Derate/Stop) and safety vector without actuation.",
  inputSchema: {
    type: "object",
    properties: {
      corridor_id: { type: "string" },
      telemetry: {
        type: "object",
        properties: {
          velocity_mps: { type: "number" },
          turbulence_index: { type: "number" },
          shear_index: { type: "number" },
          shutter_open_frac: { type: "number" },
          shutter_rate_per_min: { type: "number" },
          mobility_index: { type: "number" },
          geom_stability_index: { type: "number" },
        },
        required: [
          "velocity_mps",
          "turbulence_index",
          "shear_index",
          "shutter_open_frac",
          "shutter_rate_per_min",
          "mobility_index",
          "geom_stability_index",
        ],
      },
      prob_summary: {
        type: "object",
        properties: {
          mean: { type: "number" },
          var: { type: "number" },
          tail_prob_over_ceiling: { type: "number" },
        },
        required: ["mean", "var", "tail_prob_over_ceiling"],
      },
    },
    required: ["corridor_id", "telemetry", "prob_summary"],
  },
  outputSchema: {
    type: "object",
    properties: {
      corridor_id: { type: "string" },
      lane: { type: "string", enum: ["Allow", "Derate", "Stop"] },
      hflow_scalar: { type: "number" },
      hflow_op: { type: "number" },
      r_vel: { type: "number" },
      r_mob: { type: "number" },
      r_geo: { type: "number" },
      tail_prob_over_ceiling: { type: "number" },
      reasons: {
        type: "array",
        items: { type: "string" },
      },
      safety_vector: {
        type: "object",
        properties: {
          roh_ceiling: { type: "number" },
          soft_band_start: { type: "number" },
          hard_ceiling: { type: "number" },
          max_exceed_prob: { type: "number" },
          beta: { type: "number" },
        },
        required: [
          "roh_ceiling",
          "soft_band_start",
          "hard_ceiling",
          "max_exceed_prob",
          "beta",
        ],
      },
    },
    required: [
      "corridor_id",
      "lane",
      "hflow_scalar",
      "hflow_op",
      "r_vel",
      "r_mob",
      "r_geo",
      "tail_prob_over_ceiling",
      "reasons",
      "safety_vector",
    ],
  },
};

async function hflowCapPhxHandler(
  ctx: ToolHandlerContext,
  input: any,
): Promise<HflowGuardResult> {
  const policy = defaultCapPhxPolicy();

  const corridor_id: string = input.corridor_id;
  const telemetry: CapHflowTelemetry = input.telemetry;
  const prob_summary: HflowProbSummary = input.prob_summary;

  const { r_vel, r_mob, r_geo } = computeSubRisks(telemetry);
  const hflow_scalar = computeHflowScalar(policy, r_vel, r_mob, r_geo);
  const hflow_op = computeHflowOp(policy, prob_summary);

  const reasons: HflowReason[] = [];
  const lane = decideLane(policy, hflow_scalar, hflow_op, prob_summary.tail_prob_over_ceiling, reasons);

  return {
    corridor_id,
    hflow_scalar,
    hflow_op,
    r_vel,
    r_mob,
    r_geo,
    lane,
    reasons,
    tail_prob_over_ceiling: prob_summary.tail_prob_over_ceiling,
    safety_vector: {
      roh_ceiling: policy.hard_ceiling, // global RoH ceiling 0.30
      soft_band_start: policy.soft_band_start,
      hard_ceiling: policy.hard_ceiling,
      max_exceed_prob: policy.max_exceed_prob,
      beta: policy.beta,
    },
  };
}

// Create MCP server and register tool.
const server = new MCPServer({
  tools: [
    {
      definition: hflowCapPhxTool,
      handler: hflowCapPhxHandler,
      // Custom metadata for your governance pipeline:
      // actuationallowed=false, ecosafetyrequired=false are encoded in the catalog,
      // but you can also mirror them here if your server inspects tags.
      metadata: {
        actuationallowed: false,
        ecosafetyrequired: false,
        blast_radius: "local",
      },
    },
  ],
});

server.start().catch((err) => {
  console.error("Adaptive Corridors MCP server failed to start:", err);
  process.exit(1);
});
