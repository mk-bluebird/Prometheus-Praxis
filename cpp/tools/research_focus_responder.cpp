// File: cpp/tools/research_focus_responder.cpp
#include <iostream>

/*
 * Research focus responder for Prometheus-Praxis eco tooling.
 *
 * This tool answers the three meta-questions about research focus by
 * prioritising what will most improve eco-restoration safety and utility
 * in the current stack.
 */

int main() {
    // 1. Technical vs governance vs telemetry pipeline focus
    std::cout << "1. Research focus recommendation:\n";
    std::cout << "   Primary focus should be on the *technical implementation details*\n"
                 "   of the energy and Lyapunov residual calculations, tightly coupled\n"
                 "   with their enforcement in governance logic (ALN v2 particles).\n"
                 "   Data flow across the multi-language telemetry pipeline is important,\n"
                 "   but it should serve as evidence transport for correctly-implemented\n"
                 "   residuals and KER gates rather than being the main research target.\n\n";

    // 2. KER constraints vs schema/predicate review
    std::cout << "2. Analysis focus recommendation:\n";
    std::cout << "   Emphasise an analysis of *how the KER triad constraints shape\n"
                 "   system behavior*—in particular, how K·E−R>0 drives V_{t+1}-V_t<=-alpha*s_t\n"
                 "   and how non-offsettable planes (carbon, biodiversity, neurorights)\n"
                 "   prevent unsafe trade-offs.\n"
                 "   The database schema and routing predicate design should be reviewed\n"
                 "   as supporting artifacts, confirming they correctly encode and enforce\n"
                 "   those KER and Lyapunov invariants.\n\n";

    // 3. Output structure preference
    std::cout << "3. Output structuring preference:\n";
    std::cout << "   Prefer the output as a *cross-component integration overview*\n"
                 "   that includes:\n"
                 "     - A compliance assessment against stated invariants\n"
                 "       (Lyapunov residual non-increase, KER thresholds, corridor rules),\n"
                 "     - A qualitative performance view of workload corridor mechanics,\n"
                 "       especially around energy/carbon tailwinds,\n"
                 "     - And a clear mapping across components (C++ simulations,\n"
                 "       ALN particles, SQLite schemas, MCP tools) showing how data\n"
                 "       and predicates flow without actuation.\n";

    return 0;
}
