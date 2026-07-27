// src/browser/duties/ai-workload-ga.js
// Genetic algorithm helper for AI workload placement on cyboquatic nodes,
// with hex-anchor lane constraints baked into chromosome representation.

class WorkloadChromosome {
  constructor(taskAssignments) {
    // taskAssignments: array of { nodeId, laneId } for each task index.
    this.genes = taskAssignments.map((g) => ({
      nodeId: g.nodeId,
      laneId: g.laneId
    }));
  }

  clone() {
    return new WorkloadChromosome(this.genes);
  }
}

class WorkloadGA {
  constructor(config) {
    const defaults = {
      populationSize: 50,
      crossoverRate: 0.8,
      mutationRate: 0.1
    };
    this.config = { ...defaults, ...config };
    // allowedMap[taskIndex] = array of { nodeId, laneId } allowed pairs
    this.allowedMap = config.allowedMap || [];
    this.energyReq = config.energyReq || []; // energyreqJ_i per task
    this.carbonPerNode = config.carbonPerNode || {}; // nodeId -> carbon/J
    this.residualImprovementFn = config.residualImprovementFn; // function(chrom) -> minImprovement
  }

  // Initialize population with random feasible chromosomes.
  initPopulation(numTasks) {
    const pop = [];
    for (let p = 0; p < this.config.populationSize; p++) {
      const assignment = [];
      for (let i = 0; i < numTasks; i++) {
        const allowed = this.allowedMap[i];
        if (!allowed || allowed.length === 0) {
          throw new Error(`No allowed (node,lane) pairs for task ${i}.`);
        }
        const choice = allowed[Math.floor(Math.random() * allowed.length)];
        assignment.push({ nodeId: choice.nodeId, laneId: choice.laneId });
      }
      pop.push(new WorkloadChromosome(assignment));
    }
    this.population = pop;
  }

  // Compute objectives for a chromosome.
  evaluate(chromosome) {
    const genes = chromosome.genes;
    let totalCarbon = 0.0;
    for (let i = 0; i < genes.length; i++) {
      const nodeId = genes[i].nodeId;
      const energy = this.energyReq[i] || 0.0;
      const carbonFactor = this.carbonPerNode[nodeId] || 0.0;
      totalCarbon += energy * carbonFactor;
    }
    const minImprovement = this.residualImprovementFn
      ? this.residualImprovementFn(chromosome)
      : 0.0;
    return { totalCarbon, minImprovement };
  }

  // One-point crossover.
  crossover(parentA, parentB) {
    if (Math.random() > this.config.crossoverRate) {
      return [parentA.clone(), parentB.clone()];
    }
    const point = Math.floor(Math.random() * parentA.genes.length);
    const childAgenes = [];
    const childBgenes = [];
    for (let i = 0; i < parentA.genes.length; i++) {
      if (i < point) {
        childAgenes.push({ ...parentA.genes[i] });
        childBgenes.push({ ...parentB.genes[i] });
      } else {
        childAgenes.push({ ...parentB.genes[i] });
        childBgenes.push({ ...parentA.genes[i] });
      }
    }
    const childA = new WorkloadChromosome(childAgenes);
    const childB = new WorkloadChromosome(childBgenes);
    this._repair(childA);
    this._repair(childB);
    return [childA, childB];
  }

  // Mutation: change node or lane within allowed set.
  mutate(chromosome) {
    for (let i = 0; i < chromosome.genes.length; i++) {
      if (Math.random() < this.config.mutationRate) {
        const allowed = this.allowedMap[i];
        if (allowed && allowed.length > 0) {
          const choice = allowed[Math.floor(Math.random() * allowed.length)];
          chromosome.genes[i].nodeId = choice.nodeId;
          chromosome.genes[i].laneId = choice.laneId;
        }
      }
    }
  }

  // Repair genes to enforce hex-anchor lane constraints.
  _repair(chromosome) {
    for (let i = 0; i < chromosome.genes.length; i++) {
      const allowed = this.allowedMap[i];
      if (!allowed || allowed.length === 0) {
        continue;
      }
      const gene = chromosome.genes[i];
      const isAllowed = allowed.some(
        (a) => a.nodeId === gene.nodeId && a.laneId === gene.laneId
      );
      if (!isAllowed) {
        const choice = allowed[Math.floor(Math.random() * allowed.length)];
        gene.nodeId = choice.nodeId;
        gene.laneId = choice.laneId;
      }
    }
  }
}

export { WorkloadChromosome, WorkloadGA };
