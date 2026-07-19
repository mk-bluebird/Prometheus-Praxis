# Cyboquatic Routing, Energy, and Hydraulics Map

The diagram below outlines how non-actuating routing frames, energy mechanics, hydraulics, and ecosafety policies interact around `db_cyboquatic_daily_progress.sqlite` and related shards. It is designed as a quick orientation surface for coding-agents, AI-chat platforms, and collaborators.

```mermaid
flowchart LR
    %% Node role prefixes:
    %% inst* = instance state, def* = definition/schema, pol* = policy, env* = envelope/audit, anc* = anchor/registry.

    subgraph defHydraulics["defHydraulics · Hydraulic domain kernels"]
        defHydInflow["defHydInflow · ChannelInflows"]
        defHydDecay["defHydDecay · HydraulicDecayFrame"]
        defHydMerge["defHydMerge · ChannelMergeAccountingFrame"]
        defSATCheck["defSATCheck · SATCorridorCheckFrame"]
    end

    subgraph defEnergy["defEnergy · Energy & workload kernels"]
        defLoadDeltaVt["defLoadDeltaVt · WorkloadΔVt kernel"]
        defEnergyReq["defEnergyReq · EnergyReqJ model"]
        defLyap["defLyap · LyapunovResidual Vt"]
        defContractive["defContractive · workload_energy_contractive"]
    end

    subgraph defQuality["defQuality · Water quality & fouling"]
        defMix["defMix · QualityMixingFrame"]
        defThermal["defThermal · ThermalPlumeAuditFrame"]
        defFouling["defFouling · FoulingForecastFrame"]
    end

    subgraph instNodes["instNodes · Cyboquatic nodes & shards"]
        instNodeA["instNodeA · CAP-LP-HBUF-01"]
        instNodeB["instNodeB · GILA-RCH-HBUF-07"]
        instShards["instShards · qpudatashard rows (rx, Vt, KER, evidencehex)"]
    end

    subgraph polSpine["polSpine · Ecosafety spine & policies"]
        polCorridors["polCorridors · ecosafety.corridors.v2"]
        polKer["polKer · invariant.kerdeployable.v2.0.0"]
        polNonAct["polNonAct · NonActuatingWorkload"]
    end

    subgraph envCI["envCI · Replay & CI envelopes"]
        envReplay["envReplay · PhoenixReplayCI harness"]
        envDailyDB["envDailyDB · db_cyboquatic_daily_progress.sqlite"]
        envContractive["envContractive · workload_energy_contractive_ok flag"]
    end

    subgraph ancRegistry["ancRegistry · EcoNet index & DID anchors"]
        ancRepoIndex["ancRepoIndex · econet-index SQLite spine"]
        ancCyboScore["ancCyboScore · CyboNodeEcoScore2026v1.aln"]
        ancDID["ancDID · Bostrom DID bindings"]
    end

    %% Hydraulics → nodes
    defHydInflow --> defHydDecay
    defHydDecay --> defHydMerge
    defHydMerge --> instNodes
    instNodes --> defSATCheck
    defSATCheck --> instShards

    %% Quality & fouling → shards
    instNodes --> defMix
    instNodes --> defThermal
    instNodes --> defFouling
    defMix --> instShards
    defThermal --> instShards
    defFouling --> instShards

    %% Energy & workload → residual
    instShards --> defLoadDeltaVt
    defLoadDeltaVt --> defEnergyReq
    defEnergyReq --> defLyap
    defLyap --> instShards

    %% Contractive model and DB binding
    defLoadDeltaVt --> envDailyDB
    defEnergyReq --> envDailyDB
    defLyap --> envDailyDB
    envDailyDB --> envContractive
    envContractive --> envReplay

    %% Policies & corridors
    polCorridors --> defHydDecay
    polCorridors --> defSATCheck
    polCorridors --> defEnergyReq
    polCorridors --> defLyap

    polKer --> instShards
    polKer --> envContractive

    polNonAct --> defHydDecay
    polNonAct --> defHydMerge
    polNonAct --> defSATCheck
    polNonAct --> defMix
    polNonAct --> defThermal
    polNonAct --> defFouling
    polNonAct --> defEnergyReq
    polNonAct --> defLyap

    %% Registry & anchors
    instShards --> ancCyboScore
    ancCyboScore --> ancRepoIndex
    ancRepoIndex --> envReplay
    ancDID --> ancCyboScore
    ancDID --> instShards
```

### Reading the diagram

- Hydraulic frames (`defHyd*`) consume inflows and channel properties, compute decay and merges, and update node-bound shards with hydraulic risk and SAT performance, never actuating hardware.
- Energy frames (`defLoadDeltaVt`, `defEnergyReq`, `defLyap`, `defContractive`) map workload ΔVt into `energyreqJ` and Lyapunov residual `Vt`, stored in `db_cyboquatic_daily_progress.sqlite`, and surface a `workload_energy_contractive_ok` flag for CI.
- Quality and fouling frames (`defMix`, `defThermal`, `defFouling`) propagate water-quality, thermal, and fouling metrics into shards so agents can see how hydraulics and chemistry couple into risk coordinates.
- Policies (`polCorridors`, `polKer`, `polNonAct`) define corridors, deployment gates, and non-actuating invariants that all frames must obey, keeping ecosafety grammar frozen and non-weaponizable.
- Envelopes and anchors (`envCI`, `ancRegistry`) tie daily progress rows, ecoscore ALN particles, and EcoNet index entries together via DIDs, so AI-chat and coding-agents can safely traverse functions, data, and governance paths.
