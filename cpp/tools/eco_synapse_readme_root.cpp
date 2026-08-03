// File: cpp/tools/eco_synapse_readme_root.cpp
#include <iostream>

/*
 * Root synapse README printer for C++ side.
 *
 * This small tool prints a concise summary of the four synapsis modules
 * (two in C++, two in Java) and how they form a well-aligned language bridge
 * for Prometheus-Praxis eco tooling.
 */

int main() {
    std::cout << "# Prometheus-Praxis Synapsis Points (C++ and Java)\n\n";
    std::cout << "1. cpp/tools/eco_synapse_cpp_bridge.cpp\n"
              << "   - C++ analytics bridge exposing KER-style scoring via CLI and extern \"C\".\n"
              << "   - Non-actuating; used by Java through CLI+CSV and optionally JNI.\n\n";
    std::cout << "2. cpp/tools/README.md\n"
              << "   - Documents C++ eco tools and the synapse bridge role.\n\n";
    std::cout << "3. java/eco/EcoSynapseCliClient.java\n"
              << "   - Java CLI client that invokes the C++ bridge as a subprocess and parses CSV.\n\n";
    std::cout << "4. java/eco/EcoSynapseJniBridge.java\n"
              << "   - Optional JNI-based Java bridge to the same C++ function for low-latency use.\n\n";
    std::cout << "Each module directory includes a short README describing roles and wiring,\n"
              << "ensuring that C++ and Java remain well-aligned, non-actuating, and governed\n"
              << "by KER/Lyapunov constraints within the Prometheus-Praxis eco-restoration stack.\n";
    return 0;
}
