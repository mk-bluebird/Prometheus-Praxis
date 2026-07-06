"use strict";

const invariantsSpine = Object.freeze({
  rohGlobalCeiling: 0.30,
  lyapunovPolicy: {
    description: "Non-increase Lyapunov residual V_next ≤ V_current + ε across attached corridors.",
    corridors: ["air", "soil", "aquifer", "nanoswarm", "city-ops"]
  },
  psatNeurorightsTreaty: {
    mustPassForLanes: ["GOV", "EXPPROD"],
    domains: ["healthcare", "cybernetics", "critical-infra", "payments-inference"]
  }
});

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
    this.currentAssets = currentAssets;
    this.safetyInvariants = safetyInvariants;
    this.missingInvariants = missingInvariants;
    this.phoenixNodes = phoenixNodes;
    this.deploymentMode = deploymentMode;
    this.iso42001Status = iso42001Status;
    this.nistRmStatus = nistRmStatus;
    this.priorityScore = priorityScore;
  }
}

const roadmapEntries = [
  // 1. Kairos-Executor
  new ModuleRoadmapEntry({
    id: "kairos-executor",
    name: "Kairos-Executor",
    cluster: "Governance/Scheduling",
    currentAssets: {
      aln: ["prometheus-praxis-kairos-executor.v1.aln"],
      rust: [
        "crates/prometheuspraxis",
        "crates/prometheuspraxisker",
        "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
      ],
      lua: []
    },
    safetyInvariants: [
      "RoH lane profiles respected (RESEARCH/GOV/EXPPROD)",
      "Lyapunov non-increase for scheduled windows",
      "Governance bindings (Rights Kernel, PSAT, Veritas/Janus)"
    ],
    missingInvariants: [
      "Must-Stop proofs when RoH/KER/Lyapunov violated for all lanes",
      "ISO/IEC 42001 role/risk metadata at module level"
    ],
    phoenixNodes: ["canal pumps", "MAR vault scheduling", "cyboquatic mission time windows"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: false, notes: "Anchored in Sovereign Governance; needs module docs." },
    nistRmStatus: { map: true, measure: true, manage: true, notes: "Governance kernel wired; management playbooks per corridor pending." },
    priorityScore: 1
  }),

  // 2. Physis-Curator
  new ModuleRoadmapEntry({
    id: "physis-curator",
    name: "Physis-Curator",
    cluster: "Eco/City",
    currentAssets: {
      aln: ["prometheus-praxis-physis-curator.v1.aln"],
      rust: ["crates/prometheuspraxisphysis-curator"],
      lua: []
    },
    safetyInvariants: [
      "Soil Lyapunov non-increase (Vsoil_next ≤ Vsoil_current + ε)",
      "Aquifer Lyapunov non-increase",
      "PSAT profiles required for nanoswarms/heavy machinery steps"
    ],
    missingInvariants: [
      "Field-calibrated Vsoil/Vaquifer models for Phoenix sites",
      "ISO/IEC 42001 Annex IV alignment for eco-restoration workflows"
    ],
    phoenixNodes: ["canal banks", "MAR basins", "brownfields", "storm channels"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: false, notes: "Processes described in ontology; formal Annex IV mapping needed." },
    nistRmStatus: { map: true, measure: true, manage: false, notes: "Map/Measure done via metrics; Manage (corrective loops) to be encoded." },
    priorityScore: 1
  }),

  // 3. Hygeia-Orchestrator
  new ModuleRoadmapEntry({
    id: "hygeia-orchestrator",
    name: "Hygeia-Orchestrator",
    cluster: "Healthcare/Cybernetics",
    currentAssets: {
      aln: ["prometheus-praxis-hygeia-orchestrator.v1.aln"],
      rust: ["crates/prometheuspraxishealth"],
      lua: []
    },
    safetyInvariants: [
      "Clinical session envelopes with lifeforce floors and RoH ceilings",
      "PSAT neurorights binding for high-risk procedures",
      "Non-rollback of capabilities across procedures"
    ],
    missingInvariants: [
      "Per-device PSAT/KER mapping for concrete BCIs/nanoswarm regimens",
      "Formal binding to FDA/clinical trial protocols"
    ],
    phoenixNodes: ["clinic networks", "nanoswarm controllers", "BCI devices"],
    deploymentMode: "research-only",
    iso42001Status: { documented: false, notes: "Governance intent present; health-specific AIMS docs incomplete." },
    nistRmStatus: { map: true, measure: partialBool(true), manage: false, notes: "Risk classes sketched; monitoring & management incomplete." },
    priorityScore: 2
  }),

  // 4. Synthsis-Nexus
  new ModuleRoadmapEntry({
    id: "synthsis-nexus",
    name: "Synthsis-Nexus",
    cluster: "Healthcare/Cybernetics",
    currentAssets: {
      aln: ["prometheus-praxis-synthesis-nexus.v1.aln"],
      rust: ["crates/prometheuspraxissynthesis"],
      lua: []
    },
    safetyInvariants: [
      "Monotone capability vectors (no downgrade of safety/rights dimensions)",
      "PSAT-bound OTA evolution per augmentation stream",
      "Binding to consent ledger and AugFingerprint"
    ],
    missingInvariants: [
      "Normative decomposition of capability vector dimensions with Kani proofs",
      "Device-class specific envelopes for OTA updates"
    ],
    phoenixNodes: ["augmentation OTAs", "BCI firmware streams", "AugFingerprint payment corridors"],
    deploymentMode: "research-only",
    iso42001Status: { documented: false, notes: "Augmentation governance described; implementation docs pending." },
    nistRmStatus: { map: true, measure: true, manage: false, notes: "OTA risk mapping defined; manage-stage throttles still to code." },
    priorityScore: 2
  }),

  // 5. Nomos-Enforcer
  new ModuleRoadmapEntry({
    id: "nomos-enforcer",
    name: "Nomos-Enforcer",
    cluster: "Governance/Scheduling",
    currentAssets: {
      aln: ["prometheus-praxis-nomos-enforcer.v1.aln"],
      rust: ["crates/prometheuspraxistreatygates"],
      lua: []
    },
    safetyInvariants: [
      "Policy-to-action refinement against ThemisAxiom/PanEthos/Corpus Juris",
      "No plan allowed if rights/treaty envelopes would be violated",
      "Lane-wise risk classification (ISO/IEC 42001, EU AI Act)"
    ],
    missingInvariants: [
      "Full machine-readable AI Act risk class mapping per domain",
      "Expanded Kani harnesses for 'must Stop on rights risk'"
    ],
    phoenixNodes: ["eco plans", "city upgrades", "healthcare programs", "BioPay pilots"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: true, notes: "Core governance shard aligned; module docs to reference it explicitly." },
    nistRmStatus: { map: true, measure: true, manage: true, notes: "Acts as Govern/Map/Manage center; Measure hooks via metrics shard." },
    priorityScore: 1
  }),

  // 6. Aletheia-Verifier
  new ModuleRoadmapEntry({
    id: "aletheia-verifier",
    name: "Aletheia-Verifier",
    cluster: "Governance/Scheduling",
    currentAssets: {
      aln: ["prometheus-praxis-aletheia-verifier.v1.aln"],
      rust: ["crates/prometheuspraxisprovenance"],
      lua: []
    },
    safetyInvariants: [
      "Every governance decision anchored to Veritas-Chain and Janus-Veritas",
      "Latency ceilings for verification to stay within sub-second bounds",
      "No unanchored Allow/Derate decisions"
    ],
    missingInvariants: [
      "Formal latency budgets per corridor in ALN/Rust",
      "Audit-friendly indexing schemas for long-term retrieval"
    ],
    phoenixNodes: ["canal regime changes", "cyboquatic missions", "clinic procedures", "EcoNet rewards"],
    deploymentMode: "diagnostic-only",
    iso42001Status: { documented: true, notes: "Provenance Anchor design aligned to AIMS; per-module references needed." },
    nistRmStatus: { map: true, measure: true, manage: true, notes: "Central to AI RMF 'Measure' and 'Manage' functions." },
    priorityScore: 1
  }),

  // 7. Demos-Synaxis
  new ModuleRoadmapEntry({
    id: "demos-synaxis",
    name: "Demos-Synaxis",
    cluster: "Collective/Innovation",
    currentAssets: {
      aln: ["prometheus-praxis-demos-synaxis.v1.aln"],
      rust: ["crates/prometheuspraxiscollectivefeed"],
      lua: []
    },
    safetyInvariants: [
      "Community inputs cannot relax RoH, Tsafe, or neurorights envelopes",
      "Consent/anonymization invariants for mental/neurological streams",
      "Signals attach as KER evidence, not override levers"
    ],
    missingInvariants: [
      "Concrete Nosphere/Gemeinschaft/Wachstum ingestion for Phoenix",
      "Quantitative models for community-priority alignment and equity gaps"
    ],
    phoenixNodes: ["water/heat corridors", "canal restoration campaigns", "augmentation rollouts"],
    deploymentMode: "diagnostic-only",
    iso42001Status: { documented: false, notes: "Ethical intake described; technical documentation incomplete." },
    nistRmStatus: { map: true, measure: true, manage: false, notes: "Helps 'Map' social context and 'Measure' impact; manage-stage integration pending." },
    priorityScore: 2
  }),

  // 8. Pros-Optima
  new ModuleRoadmapEntry({
    id: "pros-optima",
    name: "Pros-Optima",
    cluster: "Eco/City",
    currentAssets: {
      aln: ["prometheus-praxis-poros-optima.v1.aln"],
      rust: ["crates/prometheuspraxisoptimizer"],
      lua: []
    },
    safetyInvariants: [
      "Seven-capital corridors (water, thermal, waste, biotic, neurobiome, somatic, treaty)",
      "Lyapunov/KER residual bounds for allocations",
      "No allocation allowed if treaty corridors breached"
    ],
    missingInvariants: [
      "Fielded multi-capital models for Phoenix infrastructure",
      "Stress-tested optimization under real load and uncertainty"
    ],
    phoenixNodes: ["canal operations", "FOG channels", "district energy/water grids"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: false, notes: "Optimization design documented; AIMS alignment to be formalized." },
    nistRmStatus: { map: true, measure: true, manage: partialBool(true), notes: "Optimization is part of 'Manage'; explicit RMF metadata needed." },
    priorityScore: 1
  }),

  // 9. Techn-Incubator
  new ModuleRoadmapEntry({
    id: "techn-incubator",
    name: "Techn-Incubator",
    cluster: "Collective/Innovation",
    currentAssets: {
      aln: ["prometheus-praxis-techne-incubator.v1.aln"],
      rust: ["crates/prometheuspraxispilots"],
      lua: []
    },
    safetyInvariants: [
      "Pilot lifecycle states (lab, micro-pilot, district-pilot, retired)",
      "No production promotion without ProofOfResearch and Kani-verified invariants",
      "Monotone safety envelopes across pilot evolution"
    ],
    missingInvariants: [
      "Concrete Phoenix pilot registry and governance ties",
      "Post-market surveillance hooks for eco/health/cyboquatic tools"
    ],
    phoenixNodes: ["canal/cyboquatic pilots", "clinic nanoswarm pilots", "EcoNet/credits pilots"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: true, notes: "Strong fit with AIMS lifecycle controls; needs module-level cross-references." },
    nistRmStatus: { map: true, measure: true, manage: true, notes: "Core to continuous 'Manage' and improvement loops." },
    priorityScore: 1
  }),

  // 10. Chthnios-Monitor
  new ModuleRoadmapEntry({
    id: "chthnios-monitor",
    name: "Chthnios-Monitor",
    cluster: "Eco/City",
    currentAssets: {
      aln: ["prometheus-praxis-chthonios-monitor.v1.aln"],
      rust: ["crates/prometheuspraxischthonios"],
      lua: []
    },
    safetyInvariants: [
      "Subsurface Lyapunov non-increase (soil/aquifer)",
      "No Physis-Curator scheduling when subsurface state poorly known",
      "Pollution and missing data both treated as risk contributions"
    ],
    missingInvariants: [
      "Full Phoenix subsurface sensor deployment and data ingestion",
      "Cross-corridor mapping from subsurface health to macro RoH/KER"
    ],
    phoenixNodes: ["canal banks subsurface probes", "MAR vault aquifer sensors"],
    deploymentMode: "diagnostic-only",
    iso42001Status: { documented: false, notes: "Monitoring concept documented; AIMS technical annex incomplete." },
    nistRmStatus: { map: true, measure: true, manage: false, notes: "Monitoring in place; management triggers via governance yet to wire." },
    priorityScore: 1
  }),

  // 11. Phos-Comms
  new ModuleRoadmapEntry({
    id: "phos-comms",
    name: "Phos-Comms",
    cluster: "Governance/Scheduling",
    currentAssets: {
      aln: ["prometheus-praxis-phaos-comms.v1.aln"],
      rust: ["crates/prometheuspraxiscomms"],
      lua: []
    },
    safetyInvariants: [
      "Explicit forbiddance of blacklisted primitives",
      "Whitelist of NIST-approved PQC classes",
      "Readonly governance channels for actuators"
    ],
    missingInvariants: [
      "Concrete PQC scheme assignments per corridor",
      "Latency and reliability SLAs per Phoenix domain"
    ],
    phoenixNodes: ["telemetry from canals", "cyboquatic machinery", "clinic BCIs", "city controllers"],
    deploymentMode: "diagnostic-only",
    iso42001Status: { documented: true, notes: "Crypto governance aligned; per-module comms docs pending." },
    nistRmStatus: { map: true, measure: true, manage: true, notes: "Secure comms is underpinning for RMF Map/Measure/Manage." },
    priorityScore: 1
  }),

  // 12. Stsis-Guard
  new ModuleRoadmapEntry({
    id: "stsis-guard",
    name: "Stsis-Guard",
    cluster: "Governance/Scheduling",
    currentAssets: {
      aln: ["prometheus-praxis-stasis-guard.v1.aln"],
      rust: ["crates/prometheuspraxisguards", "crates/prometheuspraxis/tests/kani_governance_invariants.rs"],
      lua: []
    },
    safetyInvariants: [
      "Allow/Derate/Stop decisions bound to RoH, Tsafe, Lyapunov, neurorights, treaty envelopes",
      "Non-rollback and capability non-degradation enforced",
      "Global RoH ceiling 0.30 respected regardless of public-good scalar"
    ],
    missingInvariants: [
      "Domain-specific guard policies for eco, health, cybernetics, payments",
      "Expanded Kani harnesses for domain-level invariants"
    ],
    phoenixNodes: ["all pilots (eco, cyboquatic, health, payments)", "city controllers"],
    deploymentMode: "planning-only",
    iso42001Status: { documented: true, notes: "Rights Verification Object and guards aligned; module explainer to add." },
    nistRmStatus: { map: true, measure: true, manage: true, notes: "Central to RMF 'Manage'; integrates Map and Measure signals." },
    priorityScore: 1
  })
];

function partialBool(value) {
  return value ? true : false;
}

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
