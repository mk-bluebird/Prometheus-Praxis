// filename: src/browser/duties/bee-corridor-duty.spec.js
// destination: mk-bluebird/Prometheus-Praxis/src/browser/duties/bee-corridor-duty.spec.js

import { describe, it, expect } from "vitest";
import { BeeCorridorDuty } from "./bee-corridor-duty.js";

function makeDuty(overrides = {}) {
  const defaultEnvelope = {
    cellId: "cell-1",
    envelopeId: "env-1",
    minHabitatAreaM2: 100.0,        // A_min_baseline
    maxSunflowerDensity: 0.010,     // corridor D_max cap
    metadata: {}
  };

  const fetchEnvelope = async (cellId) => {
    if (cellId === "cell-1") return defaultEnvelope;
    return null;
  };

  const fetchLedger = async () => {
    return [];
  };

  const duty = new BeeCorridorDuty({
    fetchEnvelope,
    fetchLedger,
    lyapunovWeights: { a: 1.0, b: 1.0, c: 1.0 },
    ...overrides
  });

  return { duty, defaultEnvelope };
}

describe("BeeCorridorDuty - Sunflower placement checks", () => {
  it("accepts a placement that respects A_min, D_max, and Lyapunov descent", async () => {
    const { duty, defaultEnvelope } = makeDuty();

    // A_min_baseline = 100 m^2, currentHabitat = 200 m^2, deltaLoss = 10
    // habitatAfter = 190 >= 100 -> OK
    // D_max = min(1/A_min, envelope.maxSunflowerDensity) = min(0.01, 0.01) = 0.01
    // footprint = 200 m^2 -> densityContribution = 1/200 = 0.005 <= 0.01 -> OK
    // Lyapunov: aΔP + bΔB <= cΔS -> 0.1 + 0.1 <= 0.5 -> OK
    const req = {
      assetId: "sf-1",
      cellId: defaultEnvelope.cellId,
      footprintAreaM2: 200.0,
      deltaHabitatLossM2: 10.0,
      deltaPollinationFlux: 0.1,
      deltaBeeAbundance: 0.1,
      deltaStress: 0.5,
      currentHabitatAreaM2: 200.0
    };

    const verdict = await duty.checkSunflowerPlacement(req);

    expect(verdict.allowed).toBe(true);
    expect(verdict.reasons).toEqual([]);
    expect(verdict.evidence.A_min_baseline).toBeCloseTo(100.0);
    expect(verdict.evidence.D_max).toBeCloseTo(0.01);
    expect(verdict.evidence.densityContribution).toBeCloseTo(1 / 200);
    expect(verdict.evidence.deltaV).toBeLessThanOrEqual(0);
  });

  it("rejects a placement that would drop habitat below A_min", async () => {
    const { duty, defaultEnvelope } = makeDuty();

    // A_min_baseline = 100 m^2, currentHabitat = 120, deltaLoss = 30
    // habitatAfter = 90 < 100 -> should be rejected
    const req = {
      assetId: "sf-2",
      cellId: defaultEnvelope.cellId,
      footprintAreaM2: 50.0,
      deltaHabitatLossM2: 30.0,
      deltaPollinationFlux: 0.0,
      deltaBeeAbundance: 0.0,
      deltaStress: 0.0,
      currentHabitatAreaM2: 120.0
    };

    const verdict = await duty.checkSunflowerPlacement(req);

    expect(verdict.allowed).toBe(false);
    expect(
      verdict.reasons.some((r) => r.includes("Remaining habitat"))
    ).toBe(true);
  });

  it("rejects a placement that exceeds D_max", async () => {
    const { duty, defaultEnvelope } = makeDuty();

    // A_min_baseline = 100 -> 1/A_min = 0.01; envelope.maxSunflowerDensity = 0.01
    // footprint = 50 m^2 -> densityContribution = 1/50 = 0.02 > 0.01 -> reject
    const req = {
      assetId: "sf-3",
      cellId: defaultEnvelope.cellId,
      footprintAreaM2: 50.0,
      deltaHabitatLossM2: 0.0,
      deltaPollinationFlux: 0.0,
      deltaBeeAbundance: 0.0,
      deltaStress: 1.0,
      currentHabitatAreaM2: 200.0
    };

    const verdict = await duty.checkSunflowerPlacement(req);

    expect(verdict.allowed).toBe(false);
    expect(
      verdict.reasons.some((r) => r.includes("exceeds D_max"))
    ).toBe(true);
  });

  it("rejects a placement violating Lyapunov descent (ΔV > 0)", async () => {
    const { duty, defaultEnvelope } = makeDuty();

    // A_min okay, D_max okay, but Lyapunov: aΔP + bΔB - cΔS = 1.0 > 0
    const req = {
      assetId: "sf-4",
      cellId: defaultEnvelope.cellId,
      footprintAreaM2: 200.0,
      deltaHabitatLossM2: 0.0,
      deltaPollinationFlux: 1.0,
      deltaBeeAbundance: 1.0,
      deltaStress: 1.0,
      currentHabitatAreaM2: 200.0
    };

    const verdict = await duty.checkSunflowerPlacement(req);

    expect(verdict.allowed).toBe(false);
    expect(
      verdict.reasons.some((r) => r.includes("Lyapunov descent violated"))
    ).toBe(true);
  });

  it("rejects requests with missing envelopes", async () => {
    const fetchEnvelope = async () => null;

    const duty = new BeeCorridorDuty({
      fetchEnvelope,
      lyapunovWeights: { a: 1.0, b: 1.0, c: 1.0 }
    });

    const req = {
      assetId: "sf-5",
      cellId: "unknown-cell",
      footprintAreaM2: 100.0,
      deltaHabitatLossM2: 10.0,
      deltaPollinationFlux: 0.0,
      deltaBeeAbundance: 0.0,
      deltaStress: 0.0,
      currentHabitatAreaM2: 100.0
    };

    const verdict = await duty.checkSunflowerPlacement(req);

    expect(verdict.allowed).toBe(false);
    expect(verdict.reasons[0]).toMatch(/No BeeEnvelope defined/);
  });
});

describe("BeeCorridorDuty - Nanoswarm RF checks", () => {
  it("accepts nanoswarm RF summaries below the ceiling", () => {
    const { duty } = makeDuty();

    const summary = {
      cellId: "cell-1",
      emfMaxMwPerM2: 5.0,
      rfCeilingMwPerM2: 10.0
    };

    const result = duty.checkNanoswarmRf(summary);

    expect(result.allowed).toBe(true);
    expect(result.rBeeRf).toBeCloseTo(0.5);
    expect(result.reasons.length).toBe(0);
  });

  it("rejects nanoswarm RF at or above the ceiling", () => {
    const { duty } = makeDuty();

    const summary = {
      cellId: "cell-1",
      emfMaxMwPerM2: 10.0,
      rfCeilingMwPerM2: 10.0
    };

    const result = duty.checkNanoswarmRf(summary);

    expect(result.allowed).toBe(false);
    expect(result.rBeeRf).toBeCloseTo(1.0);
    expect(
      result.reasons.some((r) => r.includes("at or above corridor ceiling"))
    ).toBe(true);
  });

  it("rejects invalid EMF inputs", () => {
    const { duty } = makeDuty();

    const summary = {
      cellId: "cell-1",
      emfMaxMwPerM2: -1.0,
      rfCeilingMwPerM2: 0.0
    };

    const result = duty.checkNanoswarmRf(summary);

    expect(result.allowed).toBe(false);
    expect(result.reasons.length).toBeGreaterThan(0);
  });
});
