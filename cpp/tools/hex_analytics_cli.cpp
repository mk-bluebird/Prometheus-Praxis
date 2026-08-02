// File: cpp/tools/hex_analytics_cli.cpp
// Unified CLI for hex analytics modules

#include <iostream>
#include <string>
#include <cstdlib>

// Include the geometric index module directly
#include "hex_anchor_geometric_index.cpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: hex_analytics_cli <mode>\n";
        std::cout << " 8 - Hex index encode/decode demo\n";
        return 1;
    }

    int mode = std::stoi(argv[1]);

    switch (mode) {
        case 8:
            return run_hex_index_demo();
        default:
            std::cout << "Unknown mode: " << mode << "\n";
            return 1;
    }
}
