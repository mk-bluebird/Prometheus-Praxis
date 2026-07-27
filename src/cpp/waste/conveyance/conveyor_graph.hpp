// File: Prometheus-Praxis/src/cpp/waste/conveyance/conveyor_graph.hpp
// License: MIT OR Apache-2.0
//
// Non-actuating telemetry header for waste conveyors and magnet nodes.
// Expresses an in-memory material-routing graph with KER, RoH ceilings,
// and EcoNet blast-radius / lane status bindings, but no actuator control.
//
// This header is designed for production telemetry pipelines and
// governance tooling. It is safe to consume from Rust FFI, ALN, and SQL
// layers and does not open any device or fieldbus interfaces.

#ifndef PROMETHEUS_PRAXIS_WASTE_CONVEYANCE_CONVEYOR_GRAPH_HPP
#define PROMETHEUS_PRAXIS_WASTE_CONVEYANCE_CONVEYOR_GRAPH_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace prometheus_praxis {
namespace waste {
namespace conveyance {

/// Lane status, aligned with EcoNet governance lanes.
/// RESEARCH: exploratory routing, high tolerance.
/// PILOT: limited production with extra guards.
/// PRODUCTION: mainline routing, strict invariants.
/// BLOCKED: corridor disabled for routing.
enum class LaneStatus : std::uint8_t {
    RESEARCH = 0,
    PILOT    = 1,
    PRODUCTION = 2,
    BLOCKED  = 3
};

/// Node type for routing graph vertices.
/// JUNCTION: generic graph node.
/// MAGNET: ferrous / non-ferrous magnet station.
/// SINK: terminal node (bin, bunker, baler, etc.).
enum class NodeType : std::uint8_t {
    JUNCTION = 0,
    MAGNET   = 1,
    SINK     = 2
};

/// Magnet band classification for magnet nodes.
/// NONE: not a magnet.
/// FERROUS: ferrous metals.
/// NONFERROUS: non-ferrous metals.
/// MIXED: mixed stream magnet configuration.
enum class MagnetBand : std::uint8_t {
    NONE        = 0,
    FERROUS     = 1,
    NONFERROUS  = 2,
    MIXED       = 3
};

/// Material class tags for segments.
/// UNKNOWN: no specific class.
/// PAPER, PLASTICS, METALS, ORGANICS: core waste classes.
/// PFAS_RISK: high persistence / PFAS-risk stream.
/// MIXED: multi-material or unsorted stream.
enum class MaterialClass : std::uint8_t {
    UNKNOWN   = 0,
    PAPER     = 1,
    PLASTICS  = 2,
    METALS    = 3,
    ORGANICS  = 4,
    PFAS_RISK = 5,
    MIXED     = 6
};

/// Simple KER triad for segments and nodes.
/// All values must be in [0.0, 1.0] and are telemetry-only.
/// k: knowledge factor (data quality, observability).
/// e: eco-impact (positive ecological benefit potential).
/// r: risk-of-harm (probability / severity of harm).
struct KerTriad {
    double k; // [0,1]
    double e; // [0,1]
    double r; // [0,1]

    KerTriad() : k(0.0), e(0.0), r(0.0) {}

    KerTriad(double k_, double e_, double r_)
        : k(k_), e(e_), r(r_) {}

    /// Clamp each axis into [0,1] to enforce invariants.
    void clamp_unit_interval() {
        k = clamp01(k);
        e = clamp01(e);
        r = clamp01(r);
    }

    /// Compute a canonical kerscore = k * e - r.
    /// This is a diagnostic scalar only; no decisions here.
    double kerscore() const {
        const double ck = clamp01(k);
        const double ce = clamp01(e);
        const double cr = clamp01(r);
        return ck * ce - cr;
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

/// RoH ceiling telemetry scalar for segments / nodes.
/// roh_ceiling: upper bound on acceptable risk-of-harm, [0,1].
struct RohCeiling {
    double roh_ceiling; // [0,1]

    RohCeiling() : roh_ceiling(0.0) {}
    explicit RohCeiling(double v) : roh_ceiling(v) {
        roh_ceiling = clamp01(roh_ceiling);
    }

    void clamp_unit_interval() {
        roh_ceiling = clamp01(roh_ceiling);
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

/// Telemetry-only conveyor segment.
/// Represents a physical conveyor or chute as a graph edge.
/// No actuation logic is present; all fields are descriptive.
struct ConveyorSegment {
    std::string   segment_id;        // unique identifier
    std::string   upstream_node_id;  // source node
    std::string   downstream_node_id;// destination node

    MaterialClass material_class;    // dominant material class
    double        nominal_load_kg_h; // nominal throughput [kg/h]
    double        peak_load_kg_h;    // peak throughput [kg/h]

    KerTriad      ker;               // KER triad [0,1]
    RohCeiling    roh;               // RoH ceiling [0,1]

    // EcoNet / blast-radius bindings (telemetry fields)
    std::string   blast_radius_plane_id; // non-offsettable plane id
    double        plane_weight;          // contribution weight [0,1]

    ConveyorSegment()
        : material_class(MaterialClass::UNKNOWN),
          nominal_load_kg_h(0.0),
          peak_load_kg_h(0.0),
          ker(),
          roh(),
          plane_weight(0.0) {}

    /// Validate invariants and clamp telemetry ranges.
    /// Throws std::invalid_argument on clearly invalid load values.
    void validate_and_normalize() {
        if (nominal_load_kg_h < 0.0 || peak_load_kg_h < 0.0) {
            throw std::invalid_argument("ConveyorSegment load must be non-negative.");
        }
        if (peak_load_kg_h < nominal_load_kg_h) {
            // For telemetry consistency, enforce peak >= nominal.
            peak_load_kg_h = nominal_load_kg_h;
        }
        ker.clamp_unit_interval();
        roh.clamp_unit_interval();
        plane_weight = clamp01(plane_weight);
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

/// Routing graph node for conveyors and magnets.
/// This is a pure telemetry representation; no device IO.
struct RoutingNode {
    std::string node_id;        // unique identifier
    NodeType    node_type;      // junction, magnet, sink
    LaneStatus  lane_status;    // EcoNet lane status

    // Magnet-specific telemetry
    MagnetBand  magnet_band;            // ferrous / non-ferrous / mixed
    double      magnet_capture_eff;     // estimated capture efficiency [0,1]
    double      magnet_contamination_risk; // contamination risk scalar [0,1]

    // Blast-radius and Lyapunov telemetry
    std::string blast_radius_plane_id;  // plane id for this node
    double      plane_weight;           // contribution weight [0,1]
    double      local_lyapunov_residual;// hint, [0,1], diagnostic only

    KerTriad    ker;                    // KER triad [0,1]
    RohCeiling  roh;                    // RoH ceiling [0,1]

    RoutingNode()
        : node_type(NodeType::JUNCTION),
          lane_status(LaneStatus::RESEARCH),
          magnet_band(MagnetBand::NONE),
          magnet_capture_eff(0.0),
          magnet_contamination_risk(0.0),
          plane_weight(0.0),
          local_lyapunov_residual(0.0),
          ker(),
          roh() {}

    /// Validate invariants and clamp telemetry ranges.
    /// Throws std::invalid_argument if magnet telemetry is inconsistent.
    void validate_and_normalize() {
        ker.clamp_unit_interval();
        roh.clamp_unit_interval();
        magnet_capture_eff        = clamp01(magnet_capture_eff);
        magnet_contamination_risk = clamp01(magnet_contamination_risk);
        plane_weight              = clamp01(plane_weight);
        local_lyapunov_residual   = clamp01(local_lyapunov_residual);

        if (node_type == NodeType::MAGNET && magnet_band == MagnetBand::NONE) {
            throw std::invalid_argument("MAGNET node must have a non-NONE magnet_band.");
        }
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }
};

/// In-memory conveyor routing graph.
/// Provides adjacency queries and read-only routing diagnostics.
/// Does NOT compute actuation plans or control devices.
class ConveyorGraph {
public:
    ConveyorGraph() = default;

    /// Add or replace a routing node.
    /// The node is validated before insertion.
    void upsert_node(const RoutingNode& node) {
        RoutingNode copy = node;
        copy.validate_and_normalize();
        nodes_[copy.node_id] = copy;
        adjacency_dirty_ = true;
    }

    /// Add or replace a conveyor segment.
    /// The segment is validated before insertion.
    void upsert_segment(const ConveyorSegment& seg) {
        ConveyorSegment copy = seg;
        copy.validate_and_normalize();
        segments_[copy.segment_id] = copy;
        adjacency_dirty_ = true;
    }

    /// Check whether a node exists.
    bool has_node(const std::string& node_id) const {
        return nodes_.find(node_id) != nodes_.end();
    }

    /// Check whether a segment exists.
    bool has_segment(const std::string& segment_id) const {
        return segments_.find(segment_id) != segments_.end();
    }

    /// Get a node by id (const reference).
    /// Throws std::out_of_range if not present.
    const RoutingNode& node(const std::string& node_id) const {
        auto it = nodes_.find(node_id);
        if (it == nodes_.end()) {
            throw std::out_of_range("RoutingNode not found: " + node_id);
        }
        return it->second;
    }

    /// Get a segment by id (const reference).
    /// Throws std::out_of_range if not present.
    const ConveyorSegment& segment(const std::string& segment_id) const {
        auto it = segments_.find(segment_id);
        if (it == segments_.end()) {
            throw std::out_of_range("ConveyorSegment not found: " + segment_id);
        }
        return it->second;
    }

    /// Return all outgoing segments from a given node (by upstream id).
    /// If the node has no outgoing segments, returns an empty vector.
    std::vector<const ConveyorSegment*> outgoing_from(
        const std::string& upstream_node_id) const
    {
        ensure_adjacency();
        std::vector<const ConveyorSegment*> result;
        auto it = adjacency_out_.find(upstream_node_id);
        if (it == adjacency_out_.end()) {
            return result;
        }
        result.reserve(it->second.size());
        for (const auto& seg_id : it->second) {
            result.push_back(&segments_.at(seg_id));
        }
        return result;
    }

    /// Return all incoming segments to a given node (by downstream id).
    /// If the node has no incoming segments, returns an empty vector.
    std::vector<const ConveyorSegment*> incoming_to(
        const std::string& downstream_node_id) const
    {
        ensure_adjacency();
        std::vector<const ConveyorSegment*> result;
        auto it = adjacency_in_.find(downstream_node_id);
        if (it == adjacency_in_.end()) {
            return result;
        }
        result.reserve(it->second.size());
        for (const auto& seg_id : it->second) {
            result.push_back(&segments_.at(seg_id));
        }
        return result;
    }

    /// List node ids that are reachable downstream from a given start node.
    /// This is a simple breadth-first traversal over the adjacency structure.
    /// It is intended for diagnostics, not for actuation planning.
    std::vector<std::string> reachable_downstream(
        const std::string& start_node_id,
        std::size_t max_depth = 32) const
    {
        ensure_adjacency();
        std::vector<std::string> result;
        if (!has_node(start_node_id) || max_depth == 0) {
            return result;
        }

        std::vector<std::string> frontier;
        frontier.push_back(start_node_id);
        std::unordered_map<std::string, bool> visited;
        visited[start_node_id] = true;

        std::size_t depth = 0;
        while (!frontier.empty() && depth < max_depth) {
            std::vector<std::string> next_frontier;
            for (const auto& nid : frontier) {
                auto it = adjacency_out_.find(nid);
                if (it == adjacency_out_.end()) {
                    continue;
                }
                for (const auto& seg_id : it->second) {
                    const ConveyorSegment& seg = segments_.at(seg_id);
                    const std::string& downstream = seg.downstream_node_id;
                    if (!visited[downstream]) {
                        visited[downstream] = true;
                        result.push_back(downstream);
                        next_frontier.push_back(downstream);
                    }
                }
            }
            frontier.swap(next_frontier);
            ++depth;
        }

        return result;
    }

    /// Export a lightweight diagnostic snapshot of lane statuses
    /// for all nodes, useful for governance dashboards.
    std::vector<std::pair<std::string, LaneStatus>> lane_status_snapshot() const {
        std::vector<std::pair<std::string, LaneStatus>> snapshot;
        snapshot.reserve(nodes_.size());
        for (const auto& kv : nodes_) {
            snapshot.emplace_back(kv.first, kv.second.lane_status);
        }
        return snapshot;
    }

private:
    std::unordered_map<std::string, RoutingNode>    nodes_;
    std::unordered_map<std::string, ConveyorSegment> segments_;

    // Adjacency maps: node_id -> vector<segment_id>.
    mutable std::unordered_map<std::string, std::vector<std::string>> adjacency_out_;
    mutable std::unordered_map<std::string, std::vector<std::string>> adjacency_in_;
    mutable bool adjacency_dirty_ = true;

    /// Rebuild adjacency maps if they are marked dirty.
    void ensure_adjacency() const {
        if (!adjacency_dirty_) {
            return;
        }

        adjacency_out_.clear();
        adjacency_in_.clear();

        for (const auto& kv : segments_) {
            const ConveyorSegment& seg = kv.second;
            adjacency_out_[seg.upstream_node_id].push_back(seg.segment_id);
            adjacency_in_[seg.downstream_node_id].push_back(seg.segment_id);
        }

        adjacency_dirty_ = false;
    }
};

} // namespace conveyance
} // namespace waste
} // namespace prometheus_praxis

#endif // PROMETHEUS_PRAXIS_WASTE_CONVEYANCE_CONVEYOR_GRAPH_HPP
