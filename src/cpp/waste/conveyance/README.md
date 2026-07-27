# Conveyors, Magnets, And Routing Graphs

This directory contains non‑actuating C++ telemetry models for waste conveyors and magnet nodes, expressed as in‑memory material‑routing graphs tied into EcoNet blast‑radius, plane weights, RoH ceilings, and Lyapunov guards.[file:6][file:8]

## Scope And Role

- Represent conveyor segments, routing nodes, and magnet stations as telemetry‑only graph elements, never actuator control paths.[file:6][file:8]
- Bind per‑segment load estimates, material class tags, KER coordinates, RoH ceilings, and blast‑radius hints into a coherent routing graph that governance crates and SQL views can consume.[file:6][file:8]
- Reference EcoNet lane status identifiers and blast‑radius planes so governance kernels can reason about research/pilot/production corridors without computing actual motion plans.[file:6][file:8]

## Core Header: conveyor_graph.hpp

- Defines `ConveyorSegment` with fields for segment ID, upstream/downstream node IDs, nominal and peak load, material class tags, RoH ceiling, and per‑segment KER coordinates in \([0,1]\).[file:6][file:8]
- Defines `RoutingNode` for junctions, magnet stations, and sinks, with node type enum, blast‑radius plane IDs, lane status (RESEARCH, PILOT, PRODUCTION, BLOCKED), and local Lyapunov residual hints.[file:6][file:8]
- Defines `ConveyorGraph` as an in‑memory adjacency structure over segments and nodes, including methods to add segments/nodes, look up neighbors, and derive read‑only routing paths for diagnostics.[file:6][file:8]

## Magnet Nodes And Material Classes

- Magnet stations are modeled as `RoutingNode` variants with additional fields: magnet band (e.g., FERROUS, NONFERROUS), capture efficiency estimate, and contamination risk scalar used to inform KER and RoH.[file:6][file:8]
- Material class tags (paper, plastics, metals, organics, PFAS‑risk, etc.) are carried per segment to allow EcoNet blast‑radius and RoH kernels to weigh routing decisions and surcharge risks.[file:6][file:8]
- No physical magnet control or PLC bindings exist in this header; magnet nodes are purely telemetry descriptors used by governance math and SQL/ALN.[file:6][file:8]

## EcoNet And Governance Bindings

- Lane status identifiers mirror EcoNet governance lanes (RESEARCH, PILOT, PRODUCTION, BLOCKED) and are stored on `RoutingNode` so higher‑level Rust/ALN code can gate routing corridors.[file:6][file:8]
- Blast‑radius plane weights and non‑offsettable plane IDs are referenced from EcoNet spines, enabling per‑segment and per‑node risk‑of‑harm aggregation without redefining kernels in C++.[file:6][file:7][file:8]
- Lyapunov guard hints (local residual slices for routing corridors) are attached as telemetry scalars, leaving canonical residual computation to Rust governance crates.[file:6][file:7][file:8]

## Invariants and Non‑Actuation

- All KER coordinates in `ConveyorSegment` are clamped to \([0,1]\) and interpreted as knowledge, eco‑impact, and risk‑of‑harm scores consistent with Cyboquatic workload and drainage kernels.[file:6][file:8]
- RoH ceilings on segments and magnet nodes are stored as bounded telemetry scalars; exceeding these ceilings is detected upstream in Rust/SQL, not enforced here.[file:6][file:7][file:8]
- The header exposes no device IO, fieldbus, PLC, or actuator APIs and is explicitly non‑actuating; it serves as a telemetry library for governance, planning, and blast‑radius diagnostics only.[file:6][file:8]
