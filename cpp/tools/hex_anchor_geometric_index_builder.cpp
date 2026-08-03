// File: cpp/tools/hex_anchor_geometric_index_builder.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// This file improves hex_anchor_geometric_index.cpp by adding an R-tree-like
// spatial index to assign Phoenix parcel points to hex anchors efficiently.
// For simplicity and portability, we implement a minimal 2D R-tree variant
// without external libraries.

namespace eco {

struct Point2D {
    double x;
    double y;
};

struct Parcel {
    std::string parcel_id;
    Point2D centroid;
};

struct HexAnchor {
    std::string hex_id;
    Point2D center;
    double radius; // approximate hex radius (for circular bounding)
};

// Axis-aligned bounding box (AABB).
struct AABB {
    double xmin;
    double xmax;
    double ymin;
    double ymax;

    bool contains(const Point2D& p) const {
        return p.x >= xmin && p.x <= xmax && p.y >= ymin && p.y <= ymax;
    }

    bool intersects(const AABB& other) const {
        return !(xmax < other.xmin || other.xmax < xmin ||
                 ymax < other.ymin || other.ymax < ymin);
    }
};

// R-tree node (simple linear R-tree).
struct RTreeNode {
    bool leaf;
    AABB bbox;
    std::vector<int> children_indices; // indices into global node array
    std::vector<int> hex_indices;      // indices into hexes for leaf nodes
};

class HexRTreeIndex {
public:
    explicit HexRTreeIndex(const std::vector<HexAnchor>& hexes_)
        : hexes(hexes_) {
        build();
    }

    // Find candidate hexes whose bounding boxes contain the parcel centroid.
    std::vector<int> query(const Point2D& p) const {
        std::vector<int> result;
        query_node(0, p, result);
        return result;
    }

    // Compute AABB for a hex (circle approximation).
    static AABB hex_aabb(const HexAnchor& h) {
        return {h.center.x - h.radius,
                h.center.x + h.radius,
                h.center.y - h.radius,
                h.center.y + h.radius};
    }

private:
    std::vector<HexAnchor> hexes;
    std::vector<RTreeNode> nodes;

    void build() {
        nodes.clear();
        RTreeNode root{};
        root.leaf = true;
        root.bbox = {0,0,0,0};
        nodes.push_back(root);

        // Insert hexes one by one (simple linear R-tree with one-level children).
        for (int i = 0; i < static_cast<int>(hexes.size()); ++i) {
            AABB hb = hex_aabb(hexes[i]);
            insert_hex(0, i, hb);
        }
    }

    void insert_hex(int node_idx, int hex_idx, const AABB& hb) {
        RTreeNode& node = nodes[node_idx];
        if (node.leaf) {
            node.hex_indices.push_back(hex_idx);
            if (node.hex_indices.size() == 1) {
                node.bbox = hb;
            } else {
                expand_bbox(node.bbox, hb);
            }
        } else {
            // In this minimal version, we keep a single-level tree; leaf-only.
        }
    }

    static void expand_bbox(AABB& b, const AABB& hb) {
        if (hb.xmin < b.xmin) b.xmin = hb.xmin;
        if (hb.xmax > b.xmax) b.xmax = hb.xmax;
        if (hb.ymin < b.ymin) b.ymin = hb.ymin;
        if (hb.ymax > b.ymax) b.ymax = hb.ymax;
    }

    void query_node(int node_idx, const Point2D& p, std::vector<int>& out) const {
        const RTreeNode& node = nodes[node_idx];
        if (!node.bbox.contains(p)) {
            return;
        }
        if (node.leaf) {
            for (int hex_idx : node.hex_indices) {
                AABB hb = hex_aabb(hexes[hex_idx]);
                if (hb.contains(p)) {
                    out.push_back(hex_idx);
                }
            }
        } else {
            for (int child_idx : node.children_indices) {
                query_node(child_idx, p, out);
            }
        }
    }
};

// Assign parcels to hexes using geometric index.
struct ParcelHexAssignment {
    std::string parcel_id;
    std::string hex_id;
};

std::vector<ParcelHexAssignment> assign_parcels_to_hexes(
        const std::vector<Parcel>& parcels,
        const std::vector<HexAnchor>& hexes) {
    HexRTreeIndex index(hexes);
    std::vector<ParcelHexAssignment> assignments;

    for (const auto& parcel : parcels) {
        auto candidates = index.query(parcel.centroid);
        if (!candidates.empty()) {
            int hex_idx = candidates.front(); // choose first candidate; refine if needed
            assignments.push_back({parcel.parcel_id, hexes[hex_idx].hex_id});
        }
    }
    return assignments;
}

// Emit SQL for hex_anchor parcel assignments.
void emit_assignments_sql(const std::vector<ParcelHexAssignment>& assigns) {
    std::cout << "DELETE FROM phoenix_hex_parcel_assignment;\n";
    for (const auto& a : assigns) {
        std::cout << "INSERT INTO phoenix_hex_parcel_assignment "
                  << "(parcel_id, hex_id) VALUES ('"
                  << a.parcel_id << "', '"
                  << a.hex_id << "');\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example Phoenix parcel centroids and hex anchors.
    std::vector<Parcel> parcels = {
        {"parcel_001", {100.0, 200.0}},
        {"parcel_002", {105.0, 205.0}},
        {"parcel_003", {300.0, 400.0}}
    };

    std::vector<HexAnchor> hexes = {
        {"hex_PHX_1", {100.0, 200.0}, 20.0},
        {"hex_PHX_2", {300.0, 400.0}, 30.0}
    };

    auto assigns = assign_parcels_to_hexes(parcels, hexes);

    std::cout << "Hex-anchor geometric index assignments:\n";
    for (const auto& a : assigns) {
        std::cout << "  parcel " << a.parcel_id << " -> " << a.hex_id << "\n";
    }
    std::cout << "\n";
    emit_assignments_sql(assigns);

    return 0;
}
