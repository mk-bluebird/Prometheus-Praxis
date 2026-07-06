// PrometheusPraxisRoadmapDuty
// Browser-duty module that exposes the deployment-first, invariants-spine roadmap
// as queryable objects for AI-native/agentic browsers. Non-actuating; read-only.

"use strict";

/**
 * Core invariants shared across modules.
 * These are the guardrails: RoH ceiling, Lyapunov stability, PSAT/neurorights/treaty gates.
 */
const invariantsSpine = Object.freeze({
  rohGlobalCeiling: 0.30,
  lyapunovPolicy: {
    description: "Non-increase Lyapunov residual V_next ≤ V_current + ε across attached corridors.",
    corridors: ["air", "soil", "aquifer", "nanoswarm", "city-ops"],
  },
  psatNeurorightsTreaty: {
    mustPassForLanes: ["GOV", "EXPPROD"],
    domains: ["healthcare", "cybernetics", "critical-infra", "payments-inference"],
  }
});

/**
 * Roadmap entry schema for one module cluster.
 */
class ModuleRoadmapEntry {
  constructor({
    id,
    name,
    cluster,
    currentAssets,
    safetyInvariants,
    missingInvariants,
    phoenixNodes,
    deploymentMode,
    iso42001Status,
    nistRmStatus,
    priorityScore
  }) {
    this.id = id;
    this.name = name;
    this.cluster = cluster;
    this.currentAssets = currentAssets;          // { aln: [...], rust: [...], lua: [...] }
    this.safetyInvariants = safetyInvariants;    // list of invariant descriptors
    this.missingInvariants = missingInvariants;  // list of gaps to be filled
    this.phoenixNodes = phoenixNodes;            // real-world targets (canals, clinics, cyboquatic, FOG, BCI, POS)
    this.deploymentMode = deploymentMode;        // "diagnostic-only" | "planning-only" | "research-only"
    this.iso42001Status = iso42001Status;        // { documented: boolean, notes: string }
    this.nistRmStatus = nistRmStatus;            // { map: boolean, measure: boolean, manage: boolean, notes: string }
    this.priorityScore = priorityScore;          // numeric or tier (1..3) for piloting
  }
}

/**
 * In-memory roadmap for the twelve modules (skeleton; to be enriched as you codify details).
 */
const roadmapEntries = [
  new ModuleRoadmapEntry({
    id: "kairos-executor",
    name: "Kairos-Executor",
    cluster: "Governance/Scheduling",
    currentAssets: {
      aln: ["prometheus-praxis-kairos-executor.v1.aln"],
      rust: ["crates/prometheuspraxis", "crates/prometheuspraxisker", "crates/prometheuspraxis/tests/kani_governance_invariants.rs"],
      lua: []
    },
    safetyInvariants: [
      "RoH lane profiles respected (RESEARCH/GOV/EXPPROD)",
      "Lyapunov non-increase for scheduled windows",
      "Must-pass governance bindings (Rights Kernel, PSAT, Veritas/Janus anchors)"
    ],
    missingInvariants: [
      "Formal 'must Stop when RoH/KER/Lyapunov violated' proofs for all lanes",
      "Full ISO/IEC 42001 role/risk metadata in ALN and Cargo"
    ],
    phoenixNodes: ["canal pumps", "MAR vault scheduling", "cyboquatic mission time windows"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: false, notes: "Anchors exist in Sovereign Governance; module-level docs still needed." },
    nistRmStatus: { map: true, measure: true, manage: partialBool(true), notes: "Metrics wired; management playbooks per corridor pending." },
    priorityScore: 1
  }),
  // Additional entries for Physis-Curator, Hygeia-Orchestrator, Synthsis-Nexus, etc. follow same pattern.
];

/**
 * Helper to encode partial management readiness (no strictest-wins or forbidden policy terms).
 */
function partialBool(value) {
  return value ? true : false;
}

/**
 * Browser-duty API: read-only queries for AI agents and dashboards.
 */
export class PrometheusPraxisRoadmapDuty {
  static getInvariantsSpine() {
    return invariantsSpine;
  }

  static listModules() {
    return roadmapEntries.map(entry => ({
      id: entry.id,
      name: entry.name,
      cluster: entry.cluster,
      deploymentMode: entry.deploymentMode,
      priorityScore: entry.priorityScore
    }));
  }

  static getModuleDetail(id) {
    return roadmapEntries.find(entry => entry.id === id) || null;
  }

  static filterByCluster(cluster) {
    return roadmapEntries.filter(entry => entry.cluster === cluster);
  }

  static filterByDeploymentMode(mode) {
    return roadmapEntries.filter(entry => entry.deploymentMode === mode);
  }

  static filterByPriority(maxPriority) {
    return roadmapEntries.filter(entry => entry.priorityScore <= maxPriority);
  }
}
