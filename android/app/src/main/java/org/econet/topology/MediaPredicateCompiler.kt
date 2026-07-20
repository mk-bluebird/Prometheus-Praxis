package org.econet.topology

/**
 * Minimal predicate language over canal / FOG topology, used to
 * detect "unmodeled media" reachability in a Cyboquatic network.
 *
 * This module is non-actuating: it only inspects an in-memory graph
 * and returns booleans, suitable for diagnostic FOG-router logic.
 */

// Predicate algebra for media / topology queries.
sealed class MediaPredicate {

    /**
     * Node must belong to the given regionId.
     */
    data class WithinRegion(val regionId: String) : MediaPredicate()

    /**
     * Node has at least one incident edge whose modelStatus is not "MODELED".
     *
     * maxDepth is carried for symmetry with ReachableUnmodeled; in the local
     * case we only look at depth 0 (incident edges on this node).
     */
    data class HasUnmodeledMedia(val maxDepth: Int) : MediaPredicate()

    /**
     * There exists a path of length <= maxDepth from this node to some node
     * that has incident unmodeled media (MISSING or STALE).
     */
    data class ReachableUnmodeled(val maxDepth: Int) : MediaPredicate()

    /**
     * Logical AND of two predicates.
     */
    data class And(val left: MediaPredicate, val right: MediaPredicate) : MediaPredicate()

    /**
     * Logical OR of two predicates.
     */
    data class Or(val left: MediaPredicate, val right: MediaPredicate) : MediaPredicate()

    /**
     * Logical NOT of a predicate.
     */
    data class Not(val inner: MediaPredicate) : MediaPredicate()
}

// Topology data classes: nodes, edges, and adjacency.
data class TopologyNode(
    val nodeId: String,
    val regionId: String,
    val kind: String
)

data class TopologyEdge(
    val edgeId: String,
    val fromNode: String,
    val toNode: String,
    val mediaType: String,
    /**
     * Model status for this media segment.
     * Convention: "MODELED", "MISSING", "STALE".
     */
    val modelStatus: String
)

data class TopologyGraph(
    val nodes: Map<String, TopologyNode>,
    /**
     * Adjacency list keyed by nodeId.
     * Each edge is directed (fromNode -> toNode); undirected
     * topologies can be represented by inserting both directions.
     */
    val adjacency: Map<String, List<TopologyEdge>>
)

/**
 * Compiler / evaluator from MediaPredicate AST to executable functions
 * over TopologyNode, given a TopologyGraph.
 */
class PredicateCompiler(private val graph: TopologyGraph) {

    /**
     * Compile a predicate AST into a function (TopologyNode) -> Boolean.
     * The returned lambda is pure and non-actuating.
     */
    fun compile(ast: MediaPredicate): (TopologyNode) -> Boolean {
        return { node -> evaluate(ast, node) }
    }

    /**
     * Internal recursive evaluator.
     */
    private fun evaluate(p: MediaPredicate, node: TopologyNode): Boolean =
        when (p) {
            is MediaPredicate.WithinRegion ->
                node.regionId == p.regionId

            is MediaPredicate.HasUnmodeledMedia ->
                hasUnmodeledAround(node.nodeId)

            is MediaPredicate.ReachableUnmodeled ->
                reachableUnmodeled(node.nodeId, p.maxDepth)

            is MediaPredicate.And ->
                evaluate(p.left, node) && evaluate(p.right, node)

            is MediaPredicate.Or ->
                evaluate(p.left, node) || evaluate(p.right, node)

            is MediaPredicate.Not ->
                !evaluate(p.inner, node)
        }

    /**
     * Local check: does this node have any incident edge with modelStatus != "MODELED"?
     */
    private fun hasUnmodeledAround(nodeId: String): Boolean {
        val edges = graph.adjacency[nodeId] ?: return false
        return edges.any { e -> !isModeled(e.modelStatus) }
    }

    /**
     * Reachability check: from startId, is there any node within maxDepth edges
     * that has incident unmodeled media?
     *
     * BFS over the topology adjacency, deterministic and non-actuating.
     */
    private fun reachableUnmodeled(startId: String, maxDepth: Int): Boolean {
        if (maxDepth < 0) return false

        val visited = mutableSetOf<String>()
        val queue: ArrayDeque<Pair<String, Int>> = ArrayDeque()
        queue.add(startId to 0)
        visited.add(startId)

        while (queue.isNotEmpty()) {
            val (nid, depth) = queue.removeFirst()
            if (depth > maxDepth) continue

            val edges = graph.adjacency[nid] ?: continue

            // If any incident edge at this node is unmodeled, succeed.
            if (edges.any { e -> !isModeled(e.modelStatus) }) {
                return true
            }

            // Otherwise, continue BFS.
            for (e in edges) {
                val next = e.toNode
                if (visited.add(next)) {
                    queue.add(next to (depth + 1))
                }
            }
        }

        return false
    }

    /**
     * Helper: treat only "MODELED" as modeled; everything else is unmodeled media.
     */
    private fun isModeled(status: String): Boolean =
        status.equals("MODELED", ignoreCase = true)
}

/**
 * Minimal usage example:
 *
 * - Build a tiny topology graph with some modeled and unmodeled edges.
 * - Compile a predicate "within region PHX-CANAL AND reachable_unmodeled depth <= 2".
 * - Evaluate it for each node.
 *
 * This function is for illustration / testing; in Android you would wire
 * PredicateCompiler to real ALNv2 topology snapshots instead.
 */
fun demoMediaPredicateCompiler() {
    // Example nodes.
    val n1 = TopologyNode(nodeId = "N1", regionId = "PHX-CANAL", kind = "vault")
    val n2 = TopologyNode(nodeId = "N2", regionId = "PHX-CANAL", kind = "canal")
    val n3 = TopologyNode(nodeId = "N3", regionId = "PHX-CANAL", kind = "canal")
    val n4 = TopologyNode(nodeId = "N4", regionId = "OTHER-REGION", kind = "canal")

    val nodes = listOf(n1, n2, n3, n4).associateBy { it.nodeId }

    // Example edges.
    val e12 = TopologyEdge(
        edgeId = "E12",
        fromNode = "N1",
        toNode = "N2",
        mediaType = "FOG",
        modelStatus = "MODELED"
    )
    val e23 = TopologyEdge(
        edgeId = "E23",
        fromNode = "N2",
        toNode = "N3",
        mediaType = "FOG",
        modelStatus = "STALE" // unmodeled media
    )
    val e34 = TopologyEdge(
        edgeId = "E34",
        fromNode = "N3",
        toNode = "N4",
        mediaType = "FOG",
        modelStatus = "MODELED"
    )

    // Build adjacency map.
    val adjacency = mapOf(
        "N1" to listOf(e12),
        "N2" to listOf(e23),
        "N3" to listOf(e34),
        // N4 has no outgoing edges in this toy example.
        "N4" to emptyList()
    )

    val graph = TopologyGraph(nodes = nodes, adjacency = adjacency)

    // Predicate: node in region PHX-CANAL AND can reach an unmodeled media segment within 2 hops.
    val predicateAst =
        MediaPredicate.And(
            MediaPredicate.WithinRegion(regionId = "PHX-CANAL"),
            MediaPredicate.ReachableUnmodeled(maxDepth = 2)
        )

    val compiler = PredicateCompiler(graph)
    val predicateFn = compiler.compile(predicateAst)

    // Evaluate over all nodes.
    for (node in nodes.values) {
        val matches = predicateFn(node)
        println("Node ${node.nodeId} in region ${node.regionId} matches predicate = $matches")
    }
}
