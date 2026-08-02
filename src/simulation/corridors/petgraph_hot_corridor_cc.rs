// File: src/simulation/corridors/petgraph_hot_corridor_cc.rs
// Role: Distributed graph computation for hot-corridor detection and labeling
//       on the hex grid using petgraph, with parallelization and incremental updates.

use petgraph::graph::{Graph, NodeIndex};
use petgraph::Undirected;
use std::collections::HashMap;
use std::sync::{Arc, Mutex};

/// Hex node metadata for corridor detection.
#[derive(Clone, Debug)]
pub struct HexNode {
    pub hex_id: String,
    pub uhi: f32,
}

/// Corridor ID assigned to connected hot components.
pub type CorridorId = u32;

/// Graph wrapper for hot corridors.
#[derive(Clone)]
pub struct HotCorridorGraph {
    pub graph: Graph<HexNode, (), Undirected>,
    pub node_map: HashMap<String, NodeIndex>,
}

/// Persistent corridor labeling store.
#[derive(Default)]
pub struct CorridorStore {
    /// hex_id -> corridor_id
    labels: HashMap<String, CorridorId>,
    /// next available corridor ID
    next_id: CorridorId,
}

impl CorridorStore {
    pub fn new() -> Self {
        Self {
            labels: HashMap::new(),
            next_id: 1,
        }
    }

    pub fn get_corridor_id(&self, hex_id: &str) -> Option<CorridorId> {
        self.labels.get(hex_id).copied()
    }

    pub fn set_corridor_id(&mut self, hex_id: String, id: CorridorId) {
        self.labels.insert(hex_id, id);
        if id >= self.next_id {
            self.next_id = id + 1;
        }
    }

    pub fn next_corridor_id(&mut self) -> CorridorId {
        let id = self.next_id;
        self.next_id += 1;
        id
    }
}

/// Build a hot-corridor graph from hex metrics and adjacency.
/// adjacency: hex_id -> neighbor hex_ids.
pub fn build_hot_corridor_graph(
    hex_metrics: &HashMap<String, f32>,               // hex_id -> UHI
    adjacency: &HashMap<String, Vec<String>>,
    uhi_threshold: f32,
) -> HotCorridorGraph {
    let mut graph = Graph::<HexNode, (), Undirected>::new_undirected();
    let mut node_map: HashMap<String, NodeIndex> = HashMap::new();

    // Add nodes above threshold.
    for (hex_id, &uhi) in hex_metrics.iter() {
        if uhi >= uhi_threshold {
            let idx = graph.add_node(HexNode {
                hex_id: hex_id.clone(),
                uhi,
            });
            node_map.insert(hex_id.clone(), idx);
        }
    }

    // Add edges between hot neighbors.
    for (hex_id, neighs) in adjacency.iter() {
        if let Some(&idx_i) = node_map.get(hex_id) {
            for n_id in neighs {
                if let Some(&idx_j) = node_map.get(n_id) {
                    graph.add_edge(idx_i, idx_j, ());
                }
            }
        }
    }

    HotCorridorGraph { graph, node_map }
}

/// Parallel connected-component labeling using rayon for multi-core CPU.
pub fn label_corridors_parallel(
    hot_graph: &HotCorridorGraph,
    store: &mut CorridorStore,
) {
    use petgraph::visit::Dfs;
    use rayon::prelude::*;

    let graph = &hot_graph.graph;

    // Track visited nodes.
    let mut visited = vec![false; graph.node_count()];

    // We'll collect component seeds and then process each component in parallel.
    let mut seeds: Vec<NodeIndex> = Vec::new();
    for idx in graph.node_indices() {
        if !visited[idx.index()] {
            seeds.push(idx);
            // Mark all nodes in component as visited to avoid duplicates.
            let mut dfs = Dfs::new(graph, idx);
            while let Some(nx) = dfs.next(graph) {
                visited[nx.index()] = true;
            }
        }
    }

    // Shared corridor-labeling map: hex_id -> corridor_id.
    let shared_labels: Arc<Mutex<HashMap<String, CorridorId>>> =
        Arc::new(Mutex::new(HashMap::new()));

    // Process each seed (component) in parallel.
    seeds.par_iter().for_each(|&seed| {
        let mut dfs = Dfs::new(graph, seed);
        let component_id: CorridorId; // local corridor ID, assigned atomically later

        // Reserve a corridor ID from the store (single-threaded section).
        {
            let mut guard = shared_labels.lock().unwrap();
            component_id = guard.len() as CorridorId + 1;
        }

        let mut local_hexes: Vec<String> = Vec::new();
        while let Some(nx) = dfs.next(graph) {
            let node = &graph[nx];
            local_hexes.push(node.hex_id.clone());
        }

        // Persist labels for this component.
        {
            let mut guard = shared_labels.lock().unwrap();
            for hex_id in local_hexes {
                guard.insert(hex_id, component_id);
            }
        }
    });

    // Merge back into CorridorStore with stable IDs.
    let labels = Arc::try_unwrap(shared_labels).unwrap().into_inner().unwrap();
    for (hex_id, cid) in labels {
        store.set_corridor_id(hex_id, cid);
    }
}

/// Incremental update: when new hex metrics arrive,
/// rebuild or update the hot corridor graph and re-label.
pub fn update_corridors_incremental(
    hex_metrics: &HashMap<String, f32>,
    adjacency: &HashMap<String, Vec<String>>,
    uhi_threshold: f32,
    store: &mut CorridorStore,
) {
    // For simplicity, rebuild the graph; for large grids, one could
    // adjust only affected nodes and edges.
    let hot_graph = build_hot_corridor_graph(hex_metrics, adjacency, uhi_threshold);
    label_corridors_parallel(&hot_graph, store);
}

fn main() {
    // Example hex metrics and adjacency.
    let mut hex_metrics: HashMap<String, f32> = HashMap::new();
    hex_metrics.insert("hex_10_20".into(), 7.5);
    hex_metrics.insert("hex_11_20".into(), 8.0);
    hex_metrics.insert("hex_10_21".into(), 6.5);
    hex_metrics.insert("hex_11_21".into(), 5.0);

    let mut adjacency: HashMap<String, Vec<String>> = HashMap::new();
    adjacency.insert("hex_10_20".into(), vec!["hex_11_20".into(), "hex_10_21".into()]);
    adjacency.insert("hex_11_20".into(), vec!["hex_10_20".into(), "hex_11_21".into()]);
    adjacency.insert("hex_10_21".into(), vec!["hex_10_20".into(), "hex_11_21".into()]);
    adjacency.insert("hex_11_21".into(), vec!["hex_11_20".into(), "hex_10_21".into()]);

    let mut store = CorridorStore::new();
    let uhi_threshold = 7.0;

    update_corridors_incremental(&hex_metrics, &adjacency, uhi_threshold, &mut store);

    println!("Corridor labels:");
    for (hex_id, cid) in store.labels.iter() {
        println!("  {hex_id} -> corridor {cid}");
    }
}
