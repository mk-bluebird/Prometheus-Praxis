// File: cpp/tools/eco_non_rust_interop_patterns.cpp
#include <iostream>

/*
 * Prometheus-Praxis Non-Rust Interop Pattern Catalog
 *
 * This executable prints the established interop patterns between C++,
 * Kotlin, Java, Lua, and ALN v2 for eco-restoration tooling. It is a
 * concrete artifact that future C++ modules and non-Rust tools can
 * reference to ensure consistent, governed wiring without new crates
 * or ad-hoc glue.
 */

int main() {
    std::cout << "Prometheus-Praxis Non-Rust Interop Pattern Catalog\n\n";

    std::cout << "1. C++ <-> Kotlin JNI Shared Library Pattern\n";
    std::cout
        << "   - Build C++ eco modules (PFAS corridor, blast-radius PDE, workload energy)\n"
        << "     as a shared library (e.g., libeco_restoration.so / .dll).\n"
        << "   - Export C-style functions with extern \"C\" for stable ABI, e.g.:\n"
        << "       double eco_pfas_step(const PfasState* state, const PfasParams* params);\n"
        << "   - Use Kotlin/Java JNI to load the library and call these functions from\n"
        << "     cyboquatic workload and governance CLIs.\n"
        << "   - Keep all actuation out of C++; JNI calls are non-actuating analytics only.\n\n";

    std::cout << "2. C++ <-> Java CLI + CSV Pattern\n";
    std::cout
        << "   - Use C++ tools to read scenarios (see eco_scenario_library_spec) and\n"
        << "     emit eco metrics (KER triads, Lyapunov residuals, PFAS/material scores)\n"
        << "     as CSV/JSONL via eco_serialization.cpp.\n"
        << "   - Java tools (e.g., KerReportGenerator, EcoSchemaValidator) consume CSV\n"
        << "     using standard libraries and render dashboards or reports.\n"
        << "   - This pattern avoids tight ABI coupling and keeps evidence in files.\n\n";

    std::cout << "3. C++ <-> Lua Predicate Embedding\n";
    std::cout
        << "   - C++ computes physical predicates (k, e, r, s, deltaVt, blastRisk) for\n"
        << "     each path or scenario frame.\n"
        << "   - Kotlin marshals these into a Lua environment and runs FOG scripts\n"
        << "     that decide lane and rejection based on ecosafety rules.\n"
        << "   - Lua never calls C++ directly; C++ stays a pure analytics engine.\n\n";

    std::cout << "4. C++ <-> ALN v2 Corridor Conformance\n";
    std::cout
        << "   - ALN shards define corridor families (PFAS fate, hydraulic blast-radius,\n"
        << "     material eco-impact, hex governance) as machine-checkable invariants.\n"
        << "   - C++ tools (aln_conformance_checker) read corridor parameters and\n"
        << "     simulate states, verifying that actions respect KER and Lyapunov\n"
        << "     constraints (V_{t+1} - V_t <= -alpha * s_t when s_t>0).\n"
        << "   - Results are written back as evidence rows in SQLite and referenced by ALN.\n\n";

    std::cout << "5. MCP-Indexed CLI Tools for Multi-Language Use\n";
    std::cout
        << "   - All C++, Kotlin, Java, and Lua tools are registered in mcp_file and\n"
        << "     mcp_tool with filekind ('CPP','KOTLIN','LUA','JAVA') and toolkind\n"
        << "     ('COMMAND','SQL_QUERY').\n"
        << "   - mcp_endpoint rows bind CLIs and shared libraries as endpoint_type 'CLI'\n"
        << "     or 'RUST_FN'-style entries for non-Rust languages.\n"
        << "   - AI agents use MCP to discover these tools, respect lanedefault and\n"
        << "     planebands, and call only non-actuating endpoints in RESEARCH lanes.\n\n";

    std::cout
        << "These patterns are templates: future C++ eco modules should reuse them\n"
        << "for interop, avoiding new crates and keeping all governance and KER\n"
        << "constraints encoded in SQLite and ALN rather than ad-hoc glue.\n";

    return 0;
}
