// File: cpp/tools/ai_chat_guidelines_stub.cpp
#include <iostream>

// This file exists solely to keep the C++ mono-repo layout coherent.
// The actual AI chat guidelines live in docs/ai_chat_guidelines.md,
// but we provide a tiny CLI helper here so MCP/CLI tooling can surface
// them as a discoverable C++ tool without violating non-actuation rules.

int main() {
    std::cout << "AI chat guidelines are documented in docs/ai_chat_guidelines.md.\n"
                 "Agents should treat this binary as a read-only helper that prints\n"
                 "the path; all KER and governance rules are enforced at SQL/ALN level.\n";
    return 0;
}
