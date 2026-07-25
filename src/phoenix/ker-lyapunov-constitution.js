// src/phoenix/ker-lyapunov-constitution.js
// Phoenix KER–Lyapunov constitution nucleus: RiskVector, invariant checks, eco-credit coupling.

"use strict";

const PHOENIX_UHI_FAMILY = "phoenix.uhi.hex.risk.v1";
const PHOENIX_MAR_FAMILY = "phoenix.mar.corridor.v1";

class RiskVector {
  constructor(planes) {
    // planes: { name: string, value: number, corridorMin: number, corridorMax: number, nonOffsettable: boolean }[]
    this.planes = planes.map((p) => ({
      name: p.name,
      value: typeof p.value === "number" ? p.value : 0.0,
      corridorMin: typeof p.corridorMin === "number" ? p.corridorMin : 0.0,
      corridorMax: typeof p.corridorMax === "number" ? p.corridorMax : 1.0,
      nonOffsettable: !!p.nonOffsettable,
    }));
  }

  withPlane(update) {
    const idx = this.planes.findIndex((p) => p.name === update.name);
    const next = this.planes.slice();
    const plane = {
      name: update.name,
      value: typeof update.value === "number" ? update.value : 0.0,
      corridorMin:
        typeof update.corridorMin === "number" ? update.corridorMin : 0.0,
      corridorMax:
        typeof update.corridorMax === "number" ? update.corridorMax : 1.0,
      nonOffsettable: !!update.nonOffsettable,
    };
    if (idx >= 0) {
      next[idx] = plane;
    } else {
      next.push(plane);
    }
    return new RiskVector(next);
  }

  getPlane(name) {
    return this.planes.find((p) => p.name === name) || null;
  }

  isWithinCorridors() {
    return this.planes.every(
      (p) => p.value >= p.corridorMin && p.value <= p.corridorMax
    );
  }

  hasNonOffsettableViolation() {
    return this.planes.some(
      (p) =>
        p.nonOffsettable &&
        (p.value < p.corridorMin || p.value > p.corridorMax)
    );
  }

  toJSON() {
    return { planes: this.planes };
  }
}

class LyapunovConstitution {
  constructor(config) {
    this.kWeight = typeof config.kWeight === "number" ? config.kWeight : 1.0;
    this.eWeight = typeof config.eWeight === "number" ? config.eWeight : 1.0;
    this.rWeight = typeof config.rWeight === "number" ? config.rWeight : 1.0;
    this.minStep = typeof config.minStep === "number" ? config.minStep : 0.0;
    this.familyId = config.familyId || PHOENIX_UHI_FAMILY;
  }

  computeStep(kScore, eScore, rScore) {
    const k = typeof kScore === "number" ? kScore : 0.0;
    const e = typeof eScore === "number" ? eScore : 0.0;
    const r = typeof rScore === "number" ? rScore : 0.0;
    const s = this.kWeight * k + this.eWeight * e - this.rWeight * r;
    return s < this.minStep ? this.minStep : s;
  }

  checkInvariant(vCurrent, vNext, step, riskVector) {
    const v_t = typeof vCurrent === "number" ? vCurrent : 0.0;
    const v_t1 = typeof vNext === "number" ? vNext : 0.0;
    const s_t = typeof step === "number" ? step : 0.0;
    const deltaV = v_t1 - v_t;
    const corridorsOk = riskVector.isWithinCorridors();
    const nonOffsettableBroken = riskVector.hasNonOffsettableViolation();
    const inequalityOk = deltaV <= -s_t;
    let lane;

    if (!corridorsOk || nonOffsettableBroken) {
      lane = "BlockedByCorridor";
    } else if (!inequalityOk && s_t > 0.0) {
      lane = "BlockedByInvariant";
    } else if (s_t <= 0.0) {
      lane = "ResearchOnly";
    } else {
      lane = "Deploy";
    }

    return {
      vCurrent: v_t,
      vNext: v_t1,
      step: s_t,
      deltaV,
      corridorsOk,
      nonOffsettableBroken,
      inequalityOk,
      lane,
      familyId: this.familyId,
      riskVector: riskVector.toJSON(),
    };
  }

  computeEcoCredit(vCurrent, vNext, riskVector, workloadJoules) {
    const v_t = typeof vCurrent === "number" ? vCurrent : 0.0;
    const v_t1 = typeof vNext === "number" ? vNext : 0.0;
    const deltaV = v_t1 - v_t;
    const ecoPlane =
      riskVector.getPlane("carbon") ||
      riskVector.getPlane("biodiversity") ||
      null;
    const nonOffsettableBroken = riskVector.hasNonOffsettableViolation();

    const workload =
      typeof workloadJoules === "number" && workloadJoules > 0.0
        ? workloadJoules
        : 0.0;

    let mintable = false;
    let cEco = 0.0;

    if (deltaV < 0.0 && !nonOffsettableBroken && workload > 0.0) {
      const ecoBandOk =
        !ecoPlane ||
        (ecoPlane.value >= ecoPlane.corridorMin &&
          ecoPlane.value <= ecoPlane.corridorMax);
      if (ecoBandOk) {
        const magnitude = -deltaV;
        const normalization = ecoPlane
          ? 1.0 + Math.abs(ecoPlane.value)
          : 1.0;
        cEco = (magnitude / normalization) * Math.log10(1.0 + workload);
        if (cEco < 0.0) {
          cEco = 0.0;
        }
        mintable = cEco > 0.0;
      }
    }

    return {
      mintable,
      cEco,
      deltaV,
      nonOffsettableBroken,
      workloadJoules: workload,
      familyId: this.familyId,
      riskVector: riskVector.toJSON(),
    };
  }
}

function createPhoenixUhiRiskVector(uhiTriad) {
  const rT =
    typeof uhiTriad.rT === "number" ? uhiTriad.rT : 0.0; // thermal
  const rC =
    typeof uhiTriad.rC === "number" ? uhiTriad.rC : 0.0; // canopy
  const rA =
    typeof uhiTriad.rA === "number" ? uhiTriad.rA : 0.0; // albedo

  const planes = [
    {
      name: "uhi.rT",
      value: rT,
      corridorMin: 0.0,
      corridorMax: 1.0,
      nonOffsettable: true,
    },
    {
      name: "uhi.rC",
      value: rC,
      corridorMin: 0.0,
      corridorMax: 1.0,
      nonOffsettable: false,
    },
    {
      name: "uhi.rA",
      value: rA,
      corridorMin: 0.0,
      corridorMax: 1.0,
      nonOffsettable: false,
    },
  ];

  return new RiskVector(planes);
}

function createPhoenixMarRiskVector(marState) {
  const rechargeRisk =
    typeof marState.rechargeRisk === "number" ? marState.rechargeRisk : 0.0;
  const substrateRisk =
    typeof marState.substrateRisk === "number" ? marState.substrateRisk : 0.0;

  const planes = [
    {
      name: "mar.recharge",
      value: rechargeRisk,
      corridorMin: 0.0,
      corridorMax: 1.0,
      nonOffsettable: true,
    },
    {
      name: "mar.substrate",
      value: substrateRisk,
      corridorMin: 0.0,
      corridorMax: 1.0,
      nonOffsettable: false,
    },
  ];

  return new RiskVector(planes);
}

module.exports = {
  RiskVector,
  LyapunovConstitution,
  createPhoenixUhiRiskVector,
  createPhoenixMarRiskVector,
  PHOENIX_UHI_FAMILY,
  PHOENIX_MAR_FAMILY,
};
