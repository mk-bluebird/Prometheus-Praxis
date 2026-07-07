// Tab-health duty: lets other agents introspect the Cyboquatic dashboard tab
// without ever mutating state or violating ALN safestep/lifeforce/corridor rules.

export class TabHealthDuty {
  constructor(windowRef) {
    this.windowRef = windowRef;
  }

  isReady() {
    const w = this.windowRef;
    return (
      typeof w !== "undefined" &&
      w.CyboquaticQuery &&
      typeof w.CyboquaticQuery.getNode === "function"
    );
  }

  getNodeSnapshot(nodeId, maxCycles) {
    if (!this.isReady()) {
      throw new Error("Cyboquatic dashboard not initialized");
    }
    const node = this.windowRef.CyboquaticQuery.getNode(nodeId);
    const history = node.history(maxCycles || 50);
    const latest = node.latest();
    return {
      nodeId,
      latestCycle: latest ? latest.cycle_index : null,
      cycleCount: history.length
    };
  }

  // Pure check: returns invariant issues for the most recent N transitions
  checkLatestInvariants(nodeId, maxCycles, weights, lifeforceFloor) {
    if (!this.isReady()) {
      throw new Error("Cyboquatic dashboard not initialized");
    }
    const node = this.windowRef.CyboquaticQuery.getNode(nodeId);
    const history = node.history(maxCycles || 50);
    const issues = [];
    if (history.length < 2) {
      return issues;
    }
    const w = weights || {
      Rmat: 1.5,
      r_heat_local: 0.8,
      r_surcharge_head: 1.0,
      r_pathogen_conc: 1.2,
      r_cec_conc: 1.3
    };
    for (let i = 1; i < history.length; i++) {
      const cPrev = history[i - 1].cycle_index;
      const cNext = history[i].cycle_index;
      const transIssues = node.checkTransition(
        cPrev,
        cNext,
        w,
        lifeforceFloor
      );
      if (transIssues.length) {
        issues.push({
          from: cPrev,
          to: cNext,
          messages: transIssues
        });
      }
    }
    return issues;
  }
}
