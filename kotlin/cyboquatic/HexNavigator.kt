// File: kotlin/cyboquatic/HexNavigator.kt
package cyboquatic

import java.sql.Connection
import java.sql.DriverManager

data class HexAnchor(
    val hexId: String,
    val domain: String,
    val subdomain: String,
    val ownerDid: String
)

data class HexSimulationBinding(
    val hexId: String,
    val cppSimulations: List<String>,
    val rustCrates: List<String>
)

object HexNavigator {

    private fun connect(dbPath: String): Connection {
        val url = "jdbc:sqlite:$dbPath"
        return DriverManager.getConnection(url)
    }

    private fun loadHexAnchors(conn: Connection): List<HexAnchor> {
        val sql = """
            SELECT hex_id, domain, subdomain, owner_did
            FROM phoenix_hex_registry;
        """.trimIndent()

        conn.createStatement().use { st ->
            st.executeQuery(sql).use { rs ->
                val out = mutableListOf<HexAnchor>()
                while (rs.next()) {
                    out.add(
                        HexAnchor(
                            hexId = rs.getString("hex_id"),
                            domain = rs.getString("domain"),
                            subdomain = rs.getString("subdomain"),
                            ownerDid = rs.getString("owner_did")
                        )
                    )
                }
                return out
            }
        }
    }

    /**
     * Lookup applicable C++ simulations and Rust crates via MCP tool metadata.
     * We rely on naming conventions:
     * - C++ tools in mcp_tool.toolkind = 'COMMAND' with filenames containing the hex-id or domain.
     * - Rust crates via toolkind = 'RUST_FN' and planebands/domain matching the hex.
     */
    private fun loadBindings(conn: Connection, hexes: List<HexAnchor>): List<HexSimulationBinding> {
        val bindings = mutableListOf<HexSimulationBinding>()

        // Preload MCP tool metadata.
        val toolSql = """
            SELECT mt.toolname,
                   mt.toolkind,
                   mt.planebands,
                   mf.relpath,
                   mf.filename
            FROM mcp_tool mt
            JOIN mcp_file mf ON mt.fileid = mf.fileid;
        """.trimIndent()

        val tools = mutableListOf<Map<String, String>>()
        conn.createStatement().use { st ->
            st.executeQuery(toolSql).use { rs ->
                while (rs.next()) {
                    tools.add(
                        mapOf(
                            "toolname" to rs.getString("toolname"),
                            "toolkind" to rs.getString("toolkind"),
                            "planebands" to rs.getString("planebands"),
                            "relpath" to rs.getString("relpath"),
                            "filename" to rs.getString("filename")
                        )
                    )
                }
            }
        }

        for (hex in hexes) {
            val cppSim = mutableListOf<String>()
            val rustCr = mutableListOf<String>()

            for (t in tools) {
                val toolkind = t["toolkind"] ?: ""
                val planebands = t["planebands"] ?: ""
                val relpath = t["relpath"] ?: ""
                val filename = t["filename"] ?: ""
                val toolname = t["toolname"] ?: ""

                val matchesHexId = filename.contains(hex.hexId, ignoreCase = true) ||
                                   relpath.contains(hex.hexId, ignoreCase = true)
                val matchesDomain = planebands.contains(hex.domain, ignoreCase = true) ||
                                    planebands.contains(hex.subdomain, ignoreCase = true)

                if (toolkind == "COMMAND" && matchesHexId || (toolkind == "COMMAND" && matchesDomain)) {
                    if (relpath.endsWith(".cpp")) {
                        cppSim.add("$toolname ($relpath)")
                    }
                }

                if (toolkind == "RUST_FN" && matchesHexId || (toolkind == "RUST_FN" && matchesDomain)) {
                    rustCr.add(toolname)
                }
            }

            bindings.add(
                HexSimulationBinding(
                    hexId = hex.hexId,
                    cppSimulations = cppSim.distinct(),
                    rustCrates = rustCr.distinct()
                )
            )
        }

        return bindings
    }

    private fun printBindings(hexes: List<HexAnchor>, bindings: List<HexSimulationBinding>) {
        println("=== Hex Anchor Navigator ===")
        for (hex in hexes) {
            val binding = bindings.firstOrNull { it.hexId == hex.hexId }
            println("Hex ${hex.hexId} [${hex.domain}/${hex.subdomain}]")
            println("  Owner DID: ${hex.ownerDid}")
            println("  C++ simulations:")
            if (binding != null && binding.cppSimulations.isNotEmpty()) {
                for (sim in binding.cppSimulations) {
                    println("    - $sim")
                }
            } else {
                println("    (none mapped)")
            }
            println("  Rust crates/functions:")
            if (binding != null && binding.rustCrates.isNotEmpty()) {
                for (cr in binding.rustCrates) {
                    println("    - $cr")
                }
            } else {
                println("    (none mapped)")
            }
            println()
        }
    }

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.isEmpty()) {
            println("Usage: HexNavigator <sqlite-db-path>")
            return
        }
        val dbPath = args[0]
        connect(dbPath).use { conn ->
            val hexes = loadHexAnchors(conn)
            val bindings = loadBindings(conn, hexes)
            printBindings(hexes, bindings)
        }
    }
}
