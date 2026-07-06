"use strict";

/**
 * PrometheusPraxisPilotsDuty
 * Javasphere duty-module for eco_restoration_shard:
 * - Encodes Phoenix pilots as virtual objects.
 * - Links pilots to Prometheus-Praxis modules and artifacts.
 * - Provides safe, non-actuating queries for AI-browser agents.
 *
 * This module is read-only: it never triggers hardware, OTAs, or network calls.
 */

class PilotModuleBinding {
  constructor({
    moduleId,
    moduleName,
    cluster,
    mustPassInvariants,
    alnArtifacts,
    rustCrates,
    luaModules
  }) {
    this.moduleId = moduleId;
    this.moduleName = moduleName;
    this.cluster = cluster;
    this.mustPassInvariants = mustPassInvariants;
    this.alnArtifacts = alnArtifacts;
    this.rustCrates = rustCrates;
    this.luaModules = luaModules;
  }
}

class PhoenixPilot {
  constructor({
    id,
    name,
    description,
    phoenixNodes,
    scope,
    deploymentMode,
    priority,
    modulesTouched,
    governanceNotes
  }) {
    this.id = id;
    this.name = name;
    this.description = description;
    this.phoenixNodes = phoenixNodes;
    this.scope = scope;
    this.deploymentMode = deploymentMode;
    this.priority = priority;
    this.modulesTouched = modulesTouched;
    this.governanceNotes = governanceNotes;
  }

  listModuleIds() {
    return this.modulesTouched.map(m => m.moduleId);
  }

  listMustPassInvariants() {
    const all = [];
    for (const m of this.modulesTouched) {
      for (const inv of m.mustPassInvariants) {
        if (!all.includes(inv)) {
          all.push(inv);
        }
      }
    }
    return all;
  }
}

/**
 * Shared invariants spine for eco_restoration_shard pilots.
 * These are descriptive labels; enforcement logic lives in guards elsewhere.
 */
const invariantsSpine = Object.freeze({
  rohGlobalCeiling: 0.30,
  ecoLyapunovNonIncrease: true,
  treatyCorridorRespect: true,
  psatNeurorightsRequired: true,
  nonRollbackCapabilities: true
});

/**
 * Pilot catalog: five Phoenix pilots touching eco_restoration_shard and Prometheus-Praxis.
 * NOTE: These entries are non-actuating metadata, safe for AI-browser duty queries.
 */
const phoenixPilots = Object.freeze([
  new PhoenixPilot({
    id: "P1",
    name: "Phoenix canal eco regime (non-cyboquatic)",
    description:
      "Planning-only canal regime pilot focusing on water, soil, and aquifer restoration without cyboquatic hardware.",
    phoenixNodes: [
      "canal segment pumps",
      "soil probes along canal banks",
      "aquifer sensors near MAR vaults"
    ],
    scope: "Single canal segment; regime design, metrics, and governance only.",
    deploymentMode: "planning-only",
    priority: 1,
    modulesTouched: [
      new PilotModuleBinding({
        moduleId: "kairos-executor",
        moduleName: "Kairos-Executor",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "RoH ≤ 0.30 for canal corridor",
          "Lyapunov non-increase for scheduled windows",
          "Governance bindings (Rights, PSAT, Veritas/Janus)"
        ],
        alnArtifacts: ["prometheus-praxis-kairos-executor.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxis",
          "crates/prometheuspraxisker",
          "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
        ],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "physis-curator",
        moduleName: "Physis-Curator",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Soil Lyapunov non-increase",
          "Aquifer Lyapunov non-increase",
          "PSAT required for nanoswarms/heavy machinery (if later added)"
        ],
        alnArtifacts: ["prometheus-praxis-physis-curator.v1.aln"],
        rustCrates: ["crates/prometheuspraxisphysis-curator"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "chthnios-monitor",
        moduleName: "Chthnios-Monitor",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Subsurface Lyapunov non-increase",
          "No scheduling when subsurface state unknown",
          "Missing data treated as risk, not zero"
        ],
        alnArtifacts: ["prometheus-praxis-chthonios-monitor.v1.aln"],
        rustCrates: ["crates/prometheuspraxischthonios"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "pros-optima",
        moduleName: "Pros-Optima",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Seven-capital corridors respected (water, thermal, waste, biotic, neurobiome, somatic, treaty)",
          "Lyapunov/KER residual bounds for allocations",
          "No allocation that breaches treaty corridors"
        ],
        alnArtifacts: ["prometheus-praxis-poros-optima.v1.aln"],
        rustCrates: ["crates/prometheuspraxisoptimizer"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "stsis-guard",
        moduleName: "Stsis-Guard",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Allow/Derate/Stop bound to RoH/Tsafe/Lyapunov/neurorights/treaty envelopes",
          "Non-rollback of capabilities",
          "Global RoH ceiling 0.30 respected"
        ],
        alnArtifacts: ["prometheus-praxis-stasis-guard.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxisguards",
          "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
        ],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "aletheia-verifier",
        moduleName: "Aletheia-Verifier",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Every regime change anchored to Veritas-Chain/Janus-Veritas",
          "No unanchored Allow/Derate decisions",
          "Verification latency kept within corridor budgets"
        ],
        alnArtifacts: ["prometheus-praxis-aletheia-verifier.v1.aln"],
        rustCrates: ["crates/prometheuspraxisprovenance"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "nomos-enforcer",
        moduleName: "Nomos-Enforcer",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Policy-to-action refinement against rights and treaty envelopes",
          "Lane-wise AI-risk classification (eco corridor)",
          "Must-Stop on rights-risk violations"
        ],
        alnArtifacts: ["prometheus-praxis-nomos-enforcer.v1.aln"],
        rustCrates: ["crates/prometheuspraxistreatygates"],
        luaModules: []
      })
    ],
    governanceNotes:
      "Pilot is constrained to planning and metrics; all actuations remain outside this module and under separate, non-LLM control."
  }),

  new PhoenixPilot({
    id: "P2",
    name: "Cyboquatic remediation missions in Phoenix canals",
    description:
      "Design and governance for robot-assisted PFAS/E. coli remediation in canals, with strong ecosafety and community gates.",
    phoenixNodes: [
      "cyboquatic robots/rovers",
      "PFAS/E. coli monitoring stations",
      "community reporting and oversight channels"
    ],
    scope: "Two canal segments with cyboquatic hardware; this duty remains non-actuating and planning-focused.",
    deploymentMode: "planning-only",
    priority: 1,
    modulesTouched: [
      // Reuse several bindings from P1 plus ecosystem/community modules
      new PilotModuleBinding({
        moduleId: "physis-curator",
        moduleName: "Physis-Curator",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Soil and water Lyapunov non-increase under cyboquatic missions",
          "PSAT binding for heavy machinery",
          "RoH ≤ 0.30 for cyboquatic corridor"
        ],
        alnArtifacts: ["prometheus-praxis-physis-curator.v1.aln"],
        rustCrates: ["crates/prometheuspraxisphysis-curator"],
        luaModules: [
          "runtime/lua/prometheus-praxis/ecoindexdiagnostics.lua",
          "runtime/lua/prometheus-praxis/guard_ecogovernancegate.lua"
        ]
      }),
      new PilotModuleBinding({
        moduleId: "chthnios-monitor",
        moduleName: "Chthnios-Monitor",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Subsurface health tracked for cyboquatic zones",
          "Remediation paused when subsurface state poorly measured"
        ],
        alnArtifacts: ["prometheus-praxis-chthonios-monitor.v1.aln"],
        rustCrates: ["crates/prometheuspraxischthonios"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "pros-optima",
        moduleName: "Pros-Optima",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Multi-capital optimization (eco, energy, treaty, community) stays within safe corridors",
          "No remediation plan that increases RoH beyond ceilings"
        ],
        alnArtifacts: ["prometheus-praxis-poros-optima.v1.aln"],
        rustCrates: ["crates/prometheuspraxisoptimizer"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "stsis-guard",
        moduleName: "Stsis-Guard",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Allow/Derate/Stop decisions bound to ecosafety and neurorights for cyboquatic missions",
          "Non-rollback of protections after pilot evolution"
        ],
        alnArtifacts: ["prometheus-praxis-stasis-guard.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxisguards",
          "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
        ],
        luaModules: ["runtime/lua/prometheus-praxis/guard_ecogovernancegate.lua"]
      }),
      new PilotModuleBinding({
        moduleId: "aletheia-verifier",
        moduleName: "Aletheia-Verifier",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "All cyboquatic missions verifiably anchored on Veritas-Chain",
          "No mission allowed without provenance entry"
        ],
        alnArtifacts: ["prometheus-praxis-aletheia-verifier.v1.aln"],
        rustCrates: ["crates/prometheuspraxisprovenance"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "nomos-enforcer",
        moduleName: "Nomos-Enforcer",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "PSAT and treaty corridors enforced before mission authorization",
          "Community FPIC and protected reach gates applied"
        ],
        alnArtifacts: ["prometheus-praxis-nomos-enforcer.v1.aln"],
        rustCrates: ["crates/prometheuspraxistreatygates"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "demos-synaxis",
        moduleName: "Demos-Synaxis",
        cluster: "Collective/Innovation",
        mustPassInvariants: [
          "Community signals cannot relax RoH/Tsafe/neurorights envelopes",
          "Consent and anonymization for mental/neurological streams"
        ],
        alnArtifacts: ["prometheus-praxis-demos-synaxis.v1.aln"],
        rustCrates: ["crates/prometheuspraxiscollectivefeed"],
        luaModules: []
      })
    ],
    governanceNotes:
      "Ecosafety Lua gates and PSAT treaties are mandatory; pilot stays on planning/diagnostic side of hardware stacks."
  }),

  new PhoenixPilot({
    id: "P3",
    name: "FOG channels and trash routing eco-ops (district-level)",
    description:
      "District-level planning for FOG (fats-oils-grease) channels and trash routing, with ecoindex observability.",
    phoenixNodes: [
      "FOG interceptors",
      "trash routing controllers",
      "canyon windnets and ecoindex dashboards"
    ],
    scope: "One district; eco workloads and routing rules only.",
    deploymentMode: "planning-only",
    priority: 2,
    modulesTouched: [
      new PilotModuleBinding({
        moduleId: "physis-curator",
        moduleName: "Physis-Curator",
        cluster: "Eco/City",
        mustPassInvariants: [
          "RoH and KER corridors for FOG/trash flows",
          "Lyapunov stability for FOG channels",
          "Pollution treated as risk, not capacity"
        ],
        alnArtifacts: ["prometheus-praxis-physis-curator.v1.aln"],
        rustCrates: ["crates/prometheuspraxisphysis-curator"],
        luaModules: ["runtime/lua/prometheus-praxis/ecoindexdiagnostics.lua"]
      }),
      new PilotModuleBinding({
        moduleId: "pros-optima",
        moduleName: "Pros-Optima",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Optimization respects eco, energy, treaty corridors",
          "No FOG routing plan that increases RoH above ceiling"
        ],
        alnArtifacts: ["prometheus-praxis-poros-optima.v1.aln"],
        rustCrates: ["crates/prometheuspraxisoptimizer"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "kairos-executor",
        moduleName: "Kairos-Executor",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Scheduling windows keep Lyapunov residuals bounded",
          "Governance bindings for district eco-ops"
        ],
        alnArtifacts: ["prometheus-praxis-kairos-executor.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxis",
          "crates/prometheuspraxisker"
        ],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "stsis-guard",
        moduleName: "Stsis-Guard",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Guard decisions prevent downgrade of eco protections",
          "RoH ≤ 0.30 and non-rollback enforced"
        ],
        alnArtifacts: ["prometheus-praxis-stasis-guard.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxisguards",
          "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
        ],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "aletheia-verifier",
        moduleName: "Aletheia-Verifier",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Routing policy changes anchored to Veritas-Chain",
          "Audit-friendly logs for eco workloads"
        ],
        alnArtifacts: ["prometheus-praxis-aletheia-verifier.v1.aln"],
        rustCrates: ["crates/prometheuspraxisprovenance"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "nomos-enforcer",
        moduleName: "Nomos-Enforcer",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Treaty corridors respected for waste routing",
          "Must-Stop when rights/treaty envelopes would be violated"
        ],
        alnArtifacts: ["prometheus-praxis-nomos-enforcer.v1.aln"],
        rustCrates: ["crates/prometheuspraxistreatygates"],
        luaModules: []
      })
    ],
    governanceNotes:
      "FOG/trash logic is constrained to analytics and planning; actuators and city controllers remain under separate, non-LLM governance."
  }),

  new PhoenixPilot({
    id: "P4",
    name: "Clinic-level nanoswarm care band (chronic care & rehab)",
    description:
      "Clinic pilot for nanoswarm-based chronic care and rehab, governed by clinical envelopes and neurorights.",
    phoenixNodes: [
      "clinic networks",
      "nanoswarm controllers",
      "BCI devices and augmentation OTAs"
    ],
    scope:
      "Selected clinics; envelopes and governance only, no direct device commands from this module.",
    deploymentMode: "planning-only",
    priority: 2,
    modulesTouched: [
      new PilotModuleBinding({
        moduleId: "hygeia-orchestrator",
        moduleName: "Hygeia-Orchestrator",
        cluster: "Healthcare/Cybernetics",
        mustPassInvariants: [
          "Clinical envelopes with lifeforce floors and RoH ceilings",
          "PSAT neurorights binding for nanoswarm/BCI actions",
          "Non-rollback of host capabilities"
        ],
        alnArtifacts: ["prometheus-praxis-hygeia-orchestrator.v1.aln"],
        rustCrates: ["crates/prometheuspraxishealth"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "synthsis-nexus",
        moduleName: "Synthsis-Nexus",
        cluster: "Healthcare/Cybernetics",
        mustPassInvariants: [
          "Monotone capability vectors (no downgrade)",
          "PSAT-bound OTA evolution per augmentation stream",
          "Binding to consent ledger and AugFingerprint"
        ],
        alnArtifacts: ["prometheus-praxis-synthesis-nexus.v1.aln"],
        rustCrates: ["crates/prometheuspraxissynthesis"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "stsis-guard",
        moduleName: "Stsis-Guard",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Guard decisions bind to RoH/Tsafe/neurorights envelopes",
          "Non-rollback of clinical protections"
        ],
        alnArtifacts: ["prometheus-praxis-stasis-guard.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxisguards",
          "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
        ],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "nomos-enforcer",
        moduleName: "Nomos-Enforcer",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Policy-to-action refinement against health neurorights/treaties",
          "Must-Stop on health rights-risk"
        ],
        alnArtifacts: ["prometheus-praxis-nomos-enforcer.v1.aln"],
        rustCrates: ["crates/prometheuspraxistreatygates"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "aletheia-verifier",
        moduleName: "Aletheia-Verifier",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Clinic procedures anchored to provenance ledger",
          "Latency budgets respected for clinical verification"
        ],
        alnArtifacts: ["prometheus-praxis-aletheia-verifier.v1.aln"],
        rustCrates: ["crates/prometheuspraxisprovenance"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "phos-comms",
        moduleName: "Phos-Comms",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Forbidden crypto primitives excluded",
          "PQC-only corridors for clinic telemetry",
          "Readonly governance channels for actuators"
        ],
        alnArtifacts: ["prometheus-praxis-phaos-comms.v1.aln"],
        rustCrates: ["crates/prometheuspraxiscomms"],
        luaModules: []
      })
    ],
    governanceNotes:
      "Clinic devices enforce hardware constraints; this duty encodes envelopes and invariants only, for AI-browser planning and documentation."
  }),

  new PhoenixPilot({
    id: "P5",
    name: "EcoNet rewards and Credits overlay (data & eco actions)",
    description:
      "EcoNet/Credits overlay pilot that rewards eco actions without plutocratic scoring or neurorights violations.",
    phoenixNodes: [
      "Phoenix residents performing eco actions",
      "canal restoration crews",
      "civic reporting and oversight nodes"
    ],
    scope:
      "City-wide rewards design; no direct settlement or actuation from this duty module.",
    deploymentMode: "planning-only",
    priority: 2,
    modulesTouched: [
      new PilotModuleBinding({
        moduleId: "nomos-enforcer",
        moduleName: "Nomos-Enforcer",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Non-plutocratic, public-good-only reward routing",
          "No exclusion from basics based on tokens or inner state",
          "EcoNet corridors aligned with neurorights and treaties"
        ],
        alnArtifacts: ["prometheus-praxis-nomos-enforcer.v1.aln"],
        rustCrates: ["crates/prometheuspraxistreatygates"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "demos-synaxis",
        moduleName: "Demos-Synaxis",
        cluster: "Collective/Innovation",
        mustPassInvariants: [
          "Community inputs cannot be coerced",
          "No scoring from inner mental state",
          "Signals attach as KER evidence, not override levers"
        ],
        alnArtifacts: ["prometheus-praxis-demos-synaxis.v1.aln"],
        rustCrates: ["crates/prometheuspraxiscollectivefeed"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "techn-incubator",
        moduleName: "Techn-Incubator",
        cluster: "Collective/Innovation",
        mustPassInvariants: [
          "Pilot lifecycle states (lab/micro/district/retired) respected",
          "No promotion to production without ProofOfResearch + Kani invariants",
          "Monotone safety envelopes across pilot evolution"
        ],
        alnArtifacts: ["prometheus-praxis-techne-incubator.v1.aln"],
        rustCrates: ["crates/prometheuspraxispilots"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "pros-optima",
        moduleName: "Pros-Optima",
        cluster: "Eco/City",
        mustPassInvariants: [
          "Eco reward allocations stay within seven-capital corridors",
          "Treaty corridors respected for reward routing"
        ],
        alnArtifacts: ["prometheus-praxis-poros-optima.v1.aln"],
        rustCrates: ["crates/prometheuspraxisoptimizer"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "stsis-guard",
        moduleName: "Stsis-Guard",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Allow/Derate/Stop bound to RoH/Tsafe/neurorights/treaties for rewards systems",
          "Non-rollback of EcoNet protections"
        ],
        alnArtifacts: ["prometheus-praxis-stasis-guard.v1.aln"],
        rustCrates: [
          "crates/prometheuspraxisguards",
          "crates/prometheuspraxis/tests/kani_governance_invariants.rs"
        ],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "aletheia-verifier",
        moduleName: "Aletheia-Verifier",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Reward flows and policy changes anchored to Veritas-Chain",
          "Audit trails for eco actions and credits"
        ],
        alnArtifacts: ["prometheus-praxis-aletheia-verifier.v1.aln"],
        rustCrates: ["crates/prometheuspraxisprovenance"],
        luaModules: []
      }),
      new PilotModuleBinding({
        moduleId: "phos-comms",
        moduleName: "Phos-Comms",
        cluster: "Governance/Scheduling",
        mustPassInvariants: [
          "Secure PQC corridors for EcoNet data",
          "Readonly governance channels for external reward systems"
        ],
        alnArtifacts: ["prometheus-praxis-phaos-comms.v1.aln"],
        rustCrates: ["crates/prometheuspraxiscomms"],
        luaModules: []
      })
    ],
    governanceNotes:
      "Rewards remain non-financial and host-benefit-first; this duty encodes invariants for AI-browser planning and docs, not settlement."
  })
]);

export class PrometheusPraxisPilotsDuty {
  static listPilots() {
    return phoenixPilots.map(pilot => ({
      id: pilot.id,
      name: pilot.name,
      priority: pilot.priority,
      deploymentMode: pilot.deploymentMode,
      scope: pilot.scope
    }));
  }

  static getPilotDetail(id) {
    return phoenixPilots.find(pilot => pilot.id === id) || null;
  }

  static listPilotsTouchingModule(moduleId) {
    return phoenixPilots
      .filter(pilot => pilot.modulesTouched.some(m => m.moduleId === moduleId))
      .map(pilot => ({
        id: pilot.id,
        name: pilot.name,
        priority: pilot.priority,
        deploymentMode: pilot.deploymentMode
      }));
  }

  static listModulesForPilot(id) {
    const pilot = phoenixPilots.find(p => p.id === id);
    if (!pilot) return [];
    return pilot.modulesTouched.map(m => ({
      moduleId: m.moduleId,
      moduleName: m.moduleName,
      cluster: m.cluster,
      mustPassInvariants: m.mustPassInvariants.slice(),
      alnArtifacts: m.alnArtifacts.slice(),
      rustCrates: m.rustCrates.slice(),
      luaModules: m.luaModules.slice()
    }));
  }

  static listMustPassInvariantsForPilot(id) {
    const pilot = phoenixPilots.find(p => p.id === id);
    if (!pilot) return [];
    return pilot.listMustPassInvariants();
  }

  static getInvariantsSpine() {
    return invariantsSpine;
  }
}
