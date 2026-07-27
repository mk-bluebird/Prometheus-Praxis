// Filename: src/main/kotlin/AlnToSqlCompiler.kt

data class AlnField(
    val name: String,
    val type: String,   // "text", "float", "hex256", etc.
    val range: Pair<Double, Double>? = null
)

data class AlnConstraint(
    val expr: String    // e.g. "s >= 0.0", "did = 'bostrom18...'", "s = k*e - r +- 0.000001"
)

data class AlnParticleSpec(
    val name: String,
    val fields: List<AlnField>,
    val constraints: List<AlnConstraint>
)

class AlnToSqlCompiler {

    fun compile(spec: AlnParticleSpec, tableName: String): String {
        val sb = StringBuilder()
        sb.appendLine("CREATE TABLE IF NOT EXISTS $tableName (")
        val cols = spec.fields.map { field ->
            val sqlType = when (field.type) {
                "float"   -> "REAL"
                "text"    -> "TEXT"
                "hex256"  -> "TEXT"
                else      -> "TEXT"
            }
            val rangeCheck = field.range?.let { (min, max) ->
                " CHECK (${field.name} BETWEEN $min AND $max)"
            } ?: ""
            "    ${field.name} $sqlType NOT NULL$rangeCheck"
        }
        sb.appendLine(cols.joinToString(",\n"))
        sb.appendLine(");")

        // Generate triggers for non-trivial constraints.
        val triggerConstraints = spec.constraints.filter { c ->
            !c.expr.contains("in ") // already handled by range
        }

        if (triggerConstraints.isNotEmpty()) {
            sb.appendLine()
            sb.appendLine("CREATE TRIGGER IF NOT EXISTS trg_${tableName}_constraints")
            sb.appendLine("BEFORE INSERT ON $tableName")
            sb.appendLine("FOR EACH ROW")
            sb.appendLine("BEGIN")
            triggerConstraints.forEach { c ->
                val exprSql = translateConstraintExpression(c.expr)
                sb.appendLine(
                    """
                    SELECT CASE
                        WHEN NOT ($exprSql) THEN
                            RAISE(ABORT, 'Constraint violated: ${c.expr}')
                    END;
                    """.trimIndent()
                )
            }
            sb.appendLine("END;")
        }

        return sb.toString()
    }

    private fun translateConstraintExpression(expr: String): String {
        // Minimal translator for a subset of ALN expressions.
        return expr
            .replace("+- 0.000001", "")     // tolerance handled in separate logic if needed
            .replace("=", "=")
    }
}
