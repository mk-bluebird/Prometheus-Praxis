// filename: src/cpp/cyboquatic_node_index/cyboquatic_node_indexer.hpp
// repo: mk-bluebird/eco_restoration_shard

#pragma once

#include <string>

namespace cyboquatic_node_index {

struct IndexConfig {
    std::string sqlite_path;
};

class CyboquaticNodeIndexer {
public:
    explicit CyboquaticNodeIndexer(const IndexConfig& config);

    int run_index();

private:
    IndexConfig config_;

    void populate_nodes();
    void populate_blast_radius();
};

} // namespace cyboquatic_node_index
