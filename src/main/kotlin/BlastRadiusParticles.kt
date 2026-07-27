// Filename: src/main/kotlin/BlastRadiusParticles.kt

data class BlastRadiusSurchargeV1(
    val diagId: String,
    val nodeId: String,
    val hexId: String,
    val timestampUtc: String,
    val radiusM: Double,
    val rhydraulic: Double,
    val rbio: Double,
    val rtox: Double,
    val kerK: Double,
    val kerE: Double,
    val kerR: Double,
    val kerScore: Double,
    val vt: Double,
    val evidenceHex: String
)

data class BlastRadiusSurchargeV2(
    val diagId: String,
    val nodeId: String,
    val hexId: String,
    val timestampUtc: String,
    val radiusM: Double,
    val rhydraulic: Double,
    val rbio: Double,
    val rtox: Double,
    val kerK: Double,
    val kerE: Double,
    val kerR: Double,
    val kerScore: Double,
    val vt: Double,
    val micropollutantIndex: Double,
    val version: Int,
    val evidenceHex: String
)

object BlastRadiusMapper {

    private const val DEFAULT_MICROPOLLUTANT_INDEX = 0.0  // governance-chosen

    fun toV2FromRow(rs: java.sql.ResultSet): BlastRadiusSurchargeV2 {
        val version = rs.getInt("version")
        val diagId = rs.getString("diag_id")
        val nodeId = rs.getString("node_id")
        val hexId = rs.getString("hex_id")
        val timestampUtc = rs.getString("timestamputc")
        val radiusM = rs.getDouble("radius_m")
        val rhydraulic = rs.getDouble("rhydraulic")
        val rbio = rs.getDouble("rbio")
        val rtox = rs.getDouble("rtox")
        val kerK = rs.getDouble("kerK")
        val kerE = rs.getDouble("kerE")
        val kerR = rs.getDouble("kerR")
        val kerScore = rs.getDouble("kerScore")
        val vt = rs.getDouble("vt")
        val evidenceHex = rs.getString("evidence_hex")

        val micropollutantIndex = if (version >= 2) {
            val raw = rs.getDouble("micropollutant_index")
            if (rs.wasNull()) DEFAULT_MICROPOLLUTANT_INDEX else raw
        } else {
            DEFAULT_MICROPOLLUTANT_INDEX
        }

        return BlastRadiusSurchargeV2(
            diagId = diagId,
            nodeId = nodeId,
            hexId = hexId,
            timestampUtc = timestampUtc,
            radiusM = radiusM,
            rhydraulic = rhydraulic,
            rbio = rbio,
            rtox = rtox,
            kerK = kerK,
            kerE = kerE,
            kerR = kerR,
            kerScore = kerScore,
            vt = vt,
            micropollutantIndex = micropollutantIndex,
            version = version,
            evidenceHex = evidenceHex
        )
    }

    fun insertV2(ps: java.sql.PreparedStatement, v2: BlastRadiusSurchargeV2) {
        ps.setString(1, v2.diagId)
        ps.setString(2, v2.nodeId)
        ps.setString(3, v2.hexId)
        ps.setString(4, v2.timestampUtc)
        ps.setDouble(5, v2.radiusM)
        ps.setDouble(6, v2.rhydraulic)
        ps.setDouble(7, v2.rbio)
        ps.setDouble(8, v2.rtox)
        ps.setDouble(9, v2.kerK)
        ps.setDouble(10, v2.kerE)
        ps.setDouble(11, v2.kerR)
        ps.setDouble(12, v2.kerScore)
        ps.setDouble(13, v2.vt)
        ps.setDouble(14, v2.micropollutantIndex)
        ps.setInt(15, v2.version)
        ps.setString(16, v2.evidenceHex)
    }
}
