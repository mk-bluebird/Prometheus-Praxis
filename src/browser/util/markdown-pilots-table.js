"use strict";

/**
 * markdown-pilots-table.js
 * Javasphere helper for eco_restoration_shard.
 *
 * Generates a Markdown table of Phoenix pilots mapped to
 * Prometheus-Praxis modules and artifacts, using the
 * PrometheusPraxisPilotsDuty metadata.
 *
 * This module is read-only and non-actuating.
 */

import { PrometheusPraxisPilotsDuty } from "../duties/prometheus-praxis-pilots-duty.js";

function escapePipe(text) {
  return String(text).replace(/\|/g, "\\|");
}

/**
 * Build a single Markdown-ready description of modules and invariants
 * for a given pilot.
 */
function formatModulesForPilot(pilot) {
  const modules = PrometheusPraxisPilotsDuty.listModulesForPilot(pilot.id);
  return modules
    .map(m => {
      const modulesLine = `${m.moduleName} (${m.cluster})`;
      const invariantsLine = m.mustPassInvariants.join("; ");
      return `${modulesLine}: ${invariantsLine}`;
    })
    .join(" / ");
}

/**
 * Build ALN/Rust/Lua artifact summary strings for a given pilot.
 */
function formatArtifactsForPilot(pilot) {
  const modules = PrometheusPraxisPilotsDuty.listModulesForPilot(pilot.id);

  const alnSet = new Set();
  const rustSet = new Set();
  const luaSet = new Set();

  for (const m of modules) {
    for (const a of m.alnArtifacts) alnSet.add(a);
    for (const r of m.rustCrates) rustSet.add(r);
    for (const l of m.luaModules) luaSet.add(l);
  }

  return {
    aln: Array.from(alnSet).join(", "),
    rust: Array.from(rustSet).join(", "),
    lua: Array.from(luaSet).join(", ")
  };
}

/**
 * Generate the full Markdown table string.
 * Columns:
 * - Pilot ID
 * - Pilot name
 * - Primary modules & must-pass invariants
 * - ALN artifacts
 * - Rust crates
 * - Lua modules
 * - Phoenix nodes / scope
 * - Priority
 */
export function generatePhoenixPilotsMarkdownTable() {
  const pilots = PrometheusPraxisPilotsDuty.listPilots();

  const header = [
    "| Pilot ID | Pilot name | Modules & must-pass invariants | ALN artifacts | Rust crates | Lua modules | Phoenix nodes / scope | Priority |",
    "|---------|------------|---------------------------------|--------------|------------|------------|------------------------|----------|"
  ];

  const rows = pilots.map(pilotSummary => {
    const detail = PrometheusPraxisPilotsDuty.getPilotDetail(pilotSummary.id);
    if (!detail) {
      return "";
    }

    const modulesText = formatModulesForPilot(detail);
    const artifacts = formatArtifactsForPilot(detail);

    const phoenixScope = `${detail.phoenixNodes.join(", ")}; ${detail.scope}`;

    const cells = [
      escapePipe(detail.id),
      escapePipe(detail.name),
      escapePipe(modulesText),
      escapePipe(artifacts.aln),
      escapePipe(artifacts.rust),
      escapePipe(artifacts.lua || "—"),
      escapePipe(phoenixScope),
      String(detail.priority)
    ];

    return `| ${cells.join(" | ")} |`;
  }).filter(row => row !== "");

  return [...header, ...rows].join("\n");
}
