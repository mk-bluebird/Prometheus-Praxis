// filename: tools/bee-corridor-safety-cli.js
// destination: github.com/mk-bluebird/Prometheus-Praxis
// purpose: Node.js CLI for corridor safety reports (safe corridors, hotspots, explanations).
// usage:
//   node tools/bee-corridor-safety-cli.js summary
//   node tools/bee-corridor-safety-cli.js explain --corridor C1
// env:
//   BEE_LEDGER_BASE_URL=http://localhost:8080/api/bee-ledger

"use strict";

import { BeeCorridorLedgerClient } from "../src/browser/bee-corridor-ledger-client.js";
import { NanoswarmSafetyQuery } from "../src/browser/nanoswarm-safety-query.js";
import { BeeNanoswarmSafetyDuty } from "../src/browser/duties/bee-nanoswarm-safety-duty.js";

/**
 * Simple fetch shim for Node.js (no global fetch in older runtimes).
 */
async function nodeFetch(url, options) {
  const { default: fetch } = await import("node-fetch");
  return fetch(url, options);
}

/**
 * Print a summary report:
 * - Safe corridors for nanoswarm.
 * - Top-K risk hotspots.
 */
async function runSummaryReport(baseUrl, topK) {
  const duty = new BeeNanoswarmSafetyDuty({
    baseUrl,
    fetchImpl: nodeFetch,
  });

  const snapshot = await duty.loadSessionSafetySnapshot(topK);

  console.log("=== Bee Corridor Safety Summary ===");
  console.log("");
  console.log("Safe corridors for nanoswarm operations:");
  if (snapshot.safeCorridors.length === 0) {
    console.log("  (none)");
  } else {
    for (const c of snapshot.safeCorridors) {
      console.log(
        `  - corridor=${c.corridorId} region=${c.regionId} cell=${c.cellId} V_total=${c.vTotal.toFixed(
          4,
        )}`,
      );
    }
  }

  console.log("");
  console.log(`Top ${topK} risk hotspots by Lyapunov residual:`);
  if (snapshot.riskHotspots.length === 0) {
    console.log("  (none)");
  } else {
    for (const h of snapshot.riskHotspots) {
      console.log(
        `  - corridor=${h.corridorId} V_total=${h.vTotal.toFixed(4)}`,
      );
    }
  }
}

/**
 * Print an explanation report for a single corridor:
 * - risk vector
 * - Lyapunov residual
 * - kerDeployable status
 * - BeeRestorationCredit
 */
async function runExplainReport(baseUrl, corridorId) {
  const duty = new BeeNanoswarmSafetyDuty({
    baseUrl,
    fetchImpl: nodeFetch,
  });

  const ctx = await duty.getCorridorContext(corridorId);

  console.log(`=== Corridor Safety Explanation: ${ctx.corridorId} ===`);
  console.log("");
  console.log("Risk vector (normalized [0,1]):");
  console.log(
    `  r_contact = ${ctx.riskVector.rContact.toFixed(4)} (non-offsettable)`,
  );
  console.log(`  r_emf     = ${ctx.riskVector.rEmf.toFixed(4)}`);
  console.log(`  r_acoustic= ${ctx.riskVector.rAcoustic.toFixed(4)}`);
  console.log(`  r_thermal = ${ctx.riskVector.rThermal.toFixed(4)}`);
  console.log(`  r_chemical= ${ctx.riskVector.rChemical.toFixed(4)}`);
  console.log("");
  console.log(`Lyapunov residual V_total = ${ctx.vTotal.toFixed(6)}`);
  console.log("");
  console.log("kerDeployable decision:");
  console.log(
    `  admissible = ${ctx.kerDeployable.admissible ? "true" : "false"}`,
  );
  console.log(
    `  reason     = ${
      ctx.kerDeployable.reason ? ctx.kerDeployable.reason : "(none)"
    }`,
  );
  console.log("");
  console.log("BeeRestorationCredit:");
  console.log(`  cell_id   = ${ctx.credit.cellId}`);
  console.log(`  balance   = ${ctx.credit.balanceBrc.toFixed(3)} BRC`);
}

/**
 * Simple argument parser.
 */
function parseArgs(argv) {
  const args = argv.slice(2);
  const cmd = args[0] || "summary";
  const opts = {};
  for (let i = 1; i < args.length; i++) {
    const a = args[i];
    if (a === "--corridor" && i + 1 < args.length) {
      opts.corridorId = args[++i];
    } else if (a === "--top" && i + 1 < args.length) {
      opts.topK = parseInt(args[++i], 10);
    }
  }
  return { cmd, opts };
}

/**
 * CLI entrypoint.
 */
async function main() {
  const baseUrl =
    process.env.BEE_LEDGER_BASE_URL || "http://localhost:8080/api/bee-ledger";
  const { cmd, opts } = parseArgs(process.argv);

  try {
    if (cmd === "summary") {
      const topK = Number.isFinite(opts.topK) ? opts.topK : 5;
      await runSummaryReport(baseUrl, topK);
    } else if (cmd === "explain") {
      if (!opts.corridorId) {
        throw new Error("explain requires --corridor <ID>");
      }
      await runExplainReport(baseUrl, opts.corridorId);
    } else {
      console.error(
        `Unknown command "${cmd}". Use "summary" or "explain".`,
      );
      process.exitCode = 1;
    }
  } catch (err) {
    console.error("BeeCorridor safety CLI error:", err.message || String(err));
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main();
}
