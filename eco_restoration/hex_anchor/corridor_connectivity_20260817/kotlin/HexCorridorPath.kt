import java.util.ArrayDeque

data class HexNode(val q: Int, val r: Int) {
    val s: Int
        get() = -q - r
}

data class PathResult(
    val found: Boolean,
    val path: List<HexNode>,
    val hopCount: Int,
    val status: String
)

fun sixNeighbors(node: HexNode): List<HexNode> = listOf(
    HexNode(node.q + 1, node.r),
    HexNode(node.q - 1, node.r),
    HexNode(node.q, node.r + 1),
    HexNode(node.q, node.r - 1),
    HexNode(node.q + 1, node.r - 1),
    HexNode(node.q - 1, node.r + 1)
)

fun shortestCorridorPath(
    start: HexNode,
    goal: HexNode,
    passableNodes: Set<HexNode>
): PathResult {
    require(start in passableNodes) { "start anchor is not an active passable corridor node" }
    require(goal in passableNodes) { "goal anchor is not an active passable corridor node" }

    if (start == goal) {
        return PathResult(true, listOf(start), 0, "START_EQUALS_GOAL")
    }

    val queue = ArrayDeque<HexNode>()
    val previous = mutableMapOf<HexNode, HexNode?>()
    queue.addLast(start)
    previous[start] = null

    while (queue.isNotEmpty()) {
        val current = queue.removeFirst()

        for (neighbor in sixNeighbors(current)) {
            if (neighbor !in passableNodes || neighbor in previous) {
                continue
            }

            previous[neighbor] = current
            if (neighbor == goal) {
                val path = mutableListOf<HexNode>()
                var cursor: HexNode? = goal
                while (cursor != null) {
                    path += cursor
                    cursor = previous[cursor]
                }
                path.reverse()

                return PathResult(
                    found = true,
                    path = path,
                    hopCount = path.size - 1,
                    status = "PATH_FOUND"
                )
            }
            queue.addLast(neighbor)
        }
    }

    return PathResult(false, emptyList(), 0, "NO_PASSABLE_CONNECTIVITY_PATH")
}

fun main(args: Array<String>) {
    require(args.size >= 8 && (args.size - 4) % 2 == 0) {
        "usage: HexCorridorPathKt <start_q> <start_r> <goal_q> <goal_r> <passable_q> <passable_r> [...]"
    }

    val start = HexNode(args[0].toInt(), args[1].toInt())
    val goal = HexNode(args[2].toInt(), args[3].toInt())
    val passable = buildSet {
        var index = 4
        while (index < args.size) {
            add(HexNode(args[index].toInt(), args[index + 1].toInt()))
            index += 2
        }
    }

    val result = shortestCorridorPath(start, goal, passable)
    println("status=${result.status}")
    println("found=${result.found}")
    println("hop_count=${result.hopCount}")
    result.path.forEachIndexed { index, node ->
        println("path_$index=q=${node.q},r=${node.r},s=${node.s}")
    }
}
