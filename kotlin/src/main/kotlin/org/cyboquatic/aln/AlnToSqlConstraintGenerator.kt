// File: kotlin/src/main/kotlin/org/cyboquatic/aln/AlnToSqlConstraintGenerator.kt
package org.cyboquatic.aln

import java.io.File

/**
 * Toolchain: Kotlin script that reads an ALN v2 file (e.g., cyboquatic_workload_ker.aln),
 * parses invariant statements, and emits SQL CHECK constraints or trigger bodies.
 *
 * Goal: guarantee that any database update violating ALN invariants is rejected
 * at the DB level (SQLite), by aligning ALN semantics with SQL-level enforcement.
 */
object AlnToSqlConstraintGenerator {

    data class Invariant(
        val name: String,
        val entity: String,
        val sqlCondition: String
    )

    // Very simple parser for a constrained ALN v2 subset:
    // - Looks for lines like:
    //      invariant Name {
    //          for w in WorkloadTelemetry {
    //              assert w.ker_e <= 0.0;
    //          }
    //      }
    // - Converts them into SQL CHECK constraints on the corresponding table.
    fun parseAlnFile(path: String): List<Invariant> {
        val lines = File(path).readLines()
        val invariants = mutableListOf<Invariant>()

        var currentName: String? = null
        var currentEntity: String? = null
        val conditions = mutableListOf<String>()

        for (line in lines.map { it.trim() }) {
            when {
                line.startsWith("invariant ") && line.endsWith("{") -> {
                    currentName = line.removePrefix("invariant").removeSuffix("{").trim()
                    currentEntity = null
                    conditions.clear()
                }
                line.startsWith("for ") && " in " in line -> {
                    // Example: for w in WorkloadTelemetry {
                    val parts = line.removePrefix("for").removeSuffix("{").trim().split(" in ")
                    if (parts.size == 2) {
                        currentEntity = parts[1].trim()
                    }
                }
                line.startsWith("assert ") -> {
                    // Example: assert w.ker_e <= 0.0;
                    val expr = line.removePrefix("assert").removeSuffix(";").trim()
                    // Convert ALN variable prefix "w." to SQL "NEW." for triggers or direct column names for CHECK.
                    val sqlExpr = expr.replace("w.", "")
                    conditions.add(sqlExpr)
                }
                line.startsWith("}") && currentName != null && currentEntity != null -> {
                    if (conditions.isNotEmpty()) {
                        val sqlCondition = conditions.joinToString(" AND ")
                        invariants += Invariant(currentName!!, currentEntity!!, sqlCondition)
                    }
                    currentName = null
                    currentEntity = null
                    conditions.clear()
                }
            }
        }

        return invariants
    }

    // Emit SQL CHECK constraints or triggers for each invariant.
    // For simplicity, emit triggers of the form:
    //
    //   CREATE TRIGGER trg_<entity>_<name>_check
    //   BEFORE INSERT OR UPDATE ON <table>
    //   FOR EACH ROW
    //   BEGIN
    //       SELECT CASE WHEN NOT (<condition with NEW.>) THEN
    //           RAISE(ABORT, '<name> invariant violated')
    //       END;
    //   END;
    //
    fun emitSqlForInvariants(invariants: List<Invariant>): String {
        val sb = StringBuilder()
        for (inv in invariants) {
            val triggerName = "trg_${inv.entity}_${inv.name}_check"
            val tableName = entityToTable(inv.entity)
            val condition = inv.sqlCondition
                .replace("w.", "") // already stripped
                .replace("ker_e", "NEW.ker_e")
                .replace("ker_k", "NEW.ker_k")
                .replace("ker_r", "NEW.ker_r")

            sb.appendLine("-- Invariant ${inv.name} for entity ${inv.entity}")
            sb.appendLine("CREATE TRIGGER IF NOT EXISTS $triggerName")
            sb.appendLine("BEFORE INSERT OR UPDATE ON $tableName")
            sb.appendLine("FOR EACH ROW")
            sb.appendLine("BEGIN")
            sb.appendLine("    SELECT CASE WHEN NOT ($condition) THEN")
            sb.appendLine("        RAISE(ABORT, '${inv.name} invariant violated for $tableName')")
            sb.appendLine("    END;")
            sb.appendLine("END;")
            sb.appendLine()
        }
        return sb.toString()
    }

    // Map ALN entity names to SQL table names.
    private fun entityToTable(entity: String): String =
        when (entity) {
            "WorkloadTelemetry" -> "cyboquatic_workload_telemetry"
            "GovernanceParticle" -> "governance_particle"
            else -> entity.lowercase()
        }

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.isEmpty()) {
            println("Usage: AlnToSqlConstraintGenerator <path_to_aln_file>")
            return
        }
        val alnPath = args[0]
        val invariants = parseAlnFile(alnPath)
        val sql = emitSqlForInvariants(invariants)
        println(sql)
    }
}
