// File: cybow/src/main/kotlin/org/cyboquatic/CybowCodec.kt

package org.cyboquatic

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class CybowWorkloadFrame(
    val sampleId: String,
    val nodeId: String,
    val timestampUnixS: Long,
    val energyReqJ: Double,
    val energySurplusJ: Double,
    val hydraulicRisk: Float,
    val uncertaintyRisk: Float,
    val renergy: Float,
    val rhydraulic: Float,
    val runcertainty: Float,
    val vtBefore: Double,
    val vtAfter: Double,
    val deltaVt: Double,
    val kfactor: Float,
    val efactor: Float,
    val rfactor: Float,
    val evidenceHex: String,
    val signingHex: String,
    val logicalName: String,
    val alnAnchorHex: String
)

object CybowCodec {
    fun decode(bytes: ByteArray): CybowWorkloadFrame {
        val buf = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN)

        val magic = buf.int
        require(magic == 0x43594257) { "invalid magic" }

        val version = buf.short.toInt() and 0xFFFF
        require(version == 0x0001) { "unsupported version" }

        val frameLen = buf.int
        require(frameLen == bytes.size) { "frame length mismatch" }

        val anchorBytes = ByteArray(16)
        buf.get(anchorBytes)
        val logicalBytes = ByteArray(32)
        buf.get(logicalBytes)

        val alnAnchorHex = anchorBytes.decodeToString().trimEnd('\u0000')
        val logicalName  = logicalBytes.decodeToString().trimEnd('\u0000')

        val sidLen = buf.get().toInt() and 0xFF
        val sidBytes = ByteArray(sidLen)
        buf.get(sidBytes)
        val sampleId = sidBytes.decodeToString()

        val nidLen = buf.get().toInt() and 0xFF
        val nidBytes = ByteArray(nidLen)
        buf.get(nidBytes)
        val nodeId = nidBytes.decodeToString()

        val timestampUnixS = buf.long
        val energyReqJ     = buf.double
        val energySurplusJ = buf.double
        val hydraulicRisk  = buf.float
        val uncertaintyRisk = buf.float
        val renergy        = buf.float
        val rhydraulic     = buf.float
        val runcertainty   = buf.float
        val vtBefore       = buf.double
        val vtAfter        = buf.double
        val deltaVt        = buf.double
        val kfactor        = buf.float
        val efactor        = buf.float
        val rfactor        = buf.float

        val evidLen = buf.get().toInt() and 0xFF
        val evidBytes = ByteArray(evidLen)
        buf.get(evidBytes)
        val evidenceHex = evidBytes.decodeToString()

        val signLen = buf.get().toInt() and 0xFF
        val signBytes = ByteArray(signLen)
        buf.get(signBytes)
        val signingHex = signBytes.decodeToString()

        return CybowWorkloadFrame(
            sampleId = sampleId,
            nodeId = nodeId,
            timestampUnixS = timestampUnixS,
            energyReqJ = energyReqJ,
            energySurplusJ = energySurplusJ,
            hydraulicRisk = hydraulicRisk,
            uncertaintyRisk = uncertaintyRisk,
            renergy = renergy,
            rhydraulic = rhydraulic,
            runcertainty = runcertainty,
            vtBefore = vtBefore,
            vtAfter = vtAfter,
            deltaVt = deltaVt,
            kfactor = kfactor,
            efactor = efactor,
            rfactor = rfactor,
            evidenceHex = evidenceHex,
            signingHex = signingHex,
            logicalName = logicalName,
            alnAnchorHex = alnAnchorHex
        )
    }
}
