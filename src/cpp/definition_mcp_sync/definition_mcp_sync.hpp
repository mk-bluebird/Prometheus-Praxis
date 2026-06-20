// filename: src/cpp/definition_mcp_sync/definition_mcp_sync.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

#include <string>

namespace definition_mcp_sync {

struct SyncConfig {
    std::string sqlite_path;
    std::string repo_root;
    std::string signingdid;
};

class DefinitionMcpSynchronizer {
public:
    explicit DefinitionMcpSynchronizer(const SyncConfig& config);

    int run_sync();

private:
    SyncConfig config_;

    void sync_definition_registry();
    void sync_mcpfile();
    void sync_mcptool();
};

} // namespace definition_mcp_sync
