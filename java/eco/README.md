# java/eco module: Eco Synapse Java Clients

This directory hosts Java synapse modules for Prometheus-Praxis:

- `EcoSynapseCliClient.java`  
  - Role: Java client for the C++ eco synapse bridge.  
  - Wiring:
    - Spawns `./build/eco_synapse_cpp_bridge K E R` as a subprocess, reads its CSV output, and converts it into a `KerScore` POJO for use in dashboards, governance tools, or AI-chat backends.
    - Keeps C++ non-actuating and uses Java only for orchestration and visualisation.
    - Can be registered in MCP as a `COMMAND` tool (`filekind='JAVA'`) that invokes the C++ CLI via `mcp_endpoint` bindings.

Future Java modules (e.g., KerReportGenerator, EcoSchemaValidator) should follow this pattern: call C++ CLIs, parse CSV/JSONL, and avoid tight ABI coupling except where JNI is explicitly needed.
