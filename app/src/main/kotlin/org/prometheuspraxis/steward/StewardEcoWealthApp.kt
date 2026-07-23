// File: app/src/main/kotlin/org/prometheuspraxis/steward/StewardEcoWealthApp.kt
// Destination: Prometheus-Praxis/steward-eco-wealth-statement/app/src/main/kotlin/org/prometheuspraxis/steward/StewardEcoWealthApp.kt
// License: MIT OR Apache-2.0

package org.prometheuspraxis.steward

import java.awt.BorderLayout
import java.awt.Dimension
import java.awt.EventQueue
import java.awt.GridBagConstraints
import java.awt.GridBagLayout
import java.awt.Insets
import java.io.File
import javax.swing.*
import javax.swing.filechooser.FileNameExtensionFilter

/**
 * Kotlin desktop app for StewardEcoWealthStatement2026v1.
 *
 * Responsibilities:
 *  - Collect statement fields and DID from the steward.
 *  - Verify and sign via Rust signing enclave (native library).
 *  - Export signed ALN row ready for registry ingestion.
 */
object StewardEcoWealthApp {

    @JvmStatic
    fun main(args: Array<String>) {
        // Load native Rust signing enclave library
        // Native library must expose: JNI method used below.
        try {
            System.loadLibrary("eco_sign_enclave")
        } catch (ex: UnsatisfiedLinkError) {
            JOptionPane.showMessageDialog(
                null,
                "Failed to load Rust signing enclave native library: ${ex.message}",
                "Signing Enclave Error",
                JOptionPane.ERROR_MESSAGE
            )
        }

        EventQueue.invokeLater {
            val frame = StewardEcoWealthFrame()
            frame.isVisible = true
        }
    }
}

class StewardEcoWealthFrame : JFrame("Steward Eco Wealth Statement 2026") {

    private val didField = JTextField("bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7")
    private val stewardNameField = JTextField()
    private val regionField = JTextField("Phoenix, AZ")
    private val ecoScoreField = JTextField("0.0")
    private val carbonOffsetField = JTextField("0.0")
    private val notesArea = JTextArea(5, 40)

    private val signatureField = JTextField()
    private val signatureAlgoField = JTextField("ALN-RUST-SECP256R1")
    private val enclaveStatusLabel = JLabel("Enclave: not signed")

    init {
        defaultCloseOperation = EXIT_ON_CLOSE
        preferredSize = Dimension(720, 520)
        layout = BorderLayout()

        val formPanel = JPanel(GridBagLayout())
        val gbc = GridBagConstraints()
        gbc.insets = Insets(4, 4, 4, 4)
        gbc.fill = GridBagConstraints.HORIZONTAL

        var row = 0

        fun addLabeled(label: String, component: JComponent, width: Int = 1) {
            gbc.gridx = 0
            gbc.gridy = row
            gbc.weightx = 0.0
            formPanel.add(JLabel(label), gbc)

            gbc.gridx = 1
            gbc.gridy = row
            gbc.weightx = 1.0
            gbc.gridwidth = width
            formPanel.add(component, gbc)
            gbc.gridwidth = 1
            row++
        }

        addLabeled("Steward Name:", stewardNameField)
        addLabeled("Steward DID:", didField)
        addLabeled("Region:", regionField)
        addLabeled("Eco Wealth Score:", ecoScoreField)
        addLabeled("Carbon Offset (tCO2e):", carbonOffsetField)

        gbc.gridx = 0
        gbc.gridy = row
        gbc.weightx = 0.0
        gbc.anchor = GridBagConstraints.NORTHWEST
        formPanel.add(JLabel("Statement Notes:"), gbc)

        gbc.gridx = 1
        gbc.gridy = row
        gbc.weightx = 1.0
        gbc.weighty = 1.0
        gbc.fill = GridBagConstraints.BOTH
        val notesScroll = JScrollPane(notesArea)
        formPanel.add(notesScroll, gbc)
        gbc.fill = GridBagConstraints.HORIZONTAL
        gbc.weighty = 0.0
        row++

        val signaturePanel = JPanel(GridBagLayout())
        val sgc = GridBagConstraints()
        sgc.insets = Insets(4, 4, 4, 4)
        sgc.fill = GridBagConstraints.HORIZONTAL

        sgc.gridx = 0
        sgc.gridy = 0
        signaturePanel.add(JLabel("Signature:"), sgc)
        sgc.gridx = 1
        sgc.gridy = 0
        sgc.weightx = 1.0
        signatureField.isEditable = false
        signaturePanel.add(signatureField, sgc)

        sgc.gridx = 0
        sgc.gridy = 1
        sgc.weightx = 0.0
        signaturePanel.add(JLabel("Signature Algorithm:"), sgc)
        sgc.gridx = 1
        sgc.gridy = 1
        sgc.weightx = 1.0
        signatureAlgoField.isEditable = false
        signaturePanel.add(signatureAlgoField, sgc)

        sgc.gridx = 0
        sgc.gridy = 2
        sgc.gridwidth = 2
        signaturePanel.add(enclaveStatusLabel, sgc)

        val buttonPanel = JPanel()
        val signButton = JButton("Sign Statement")
        val exportButton = JButton("Export Signed ALN Row")
        buttonPanel.add(signButton)
        buttonPanel.add(exportButton)

        signButton.addActionListener {
            signStatement()
        }

        exportButton.addActionListener {
            exportAlnRow()
        }

        val mainPanel = JPanel(BorderLayout())
        mainPanel.add(formPanel, BorderLayout.CENTER)
        mainPanel.add(signaturePanel, BorderLayout.SOUTH)

        add(mainPanel, BorderLayout.CENTER)
        add(buttonPanel, BorderLayout.SOUTH)

        pack()
        setLocationRelativeTo(null)
    }

    private fun signStatement() {
        val did = didField.text.trim()
        val stewardName = stewardNameField.text.trim()
        val region = regionField.text.trim()
        val ecoScoreText = ecoScoreField.text.trim()
        val carbonOffsetText = carbonOffsetField.text.trim()
        val notes = notesArea.text.trim()

        if (stewardName.isEmpty() || did.isEmpty()) {
            JOptionPane.showMessageDialog(
                this,
                "Steward name and DID must not be empty.",
                "Validation Error",
                JOptionPane.ERROR_MESSAGE
            )
            return
        }

        if (!validateDidFormat(did)) {
            JOptionPane.showMessageDialog(
                this,
                "DID format is invalid for StewardEcoWealthStatement2026v1.",
                "DID Validation Error",
                JOptionPane.ERROR_MESSAGE
            )
            return
        }

        val ecoScore = ecoScoreText.toDoubleOrNull()
        val carbonOffset = carbonOffsetText.toDoubleOrNull()

        if (ecoScore == null || carbonOffset == null) {
            JOptionPane.showMessageDialog(
                this,
                "Eco Wealth Score and Carbon Offset must be numeric.",
                "Validation Error",
                JOptionPane.ERROR_MESSAGE
            )
            return
        }

        val payload = buildStatementPayload(
            stewardName = stewardName,
            did = did,
            region = region,
            ecoScore = ecoScore,
            carbonOffset = carbonOffset,
            notes = notes
        )

        try {
            val signatureHex = RoHSigningBridge.signEcoWealthStatement(did, payload)
            signatureField.text = signatureHex
            enclaveStatusLabel.text = "Enclave: signature OK (length=${signatureHex.length} hex chars)"
        } catch (ex: Exception) {
            enclaveStatusLabel.text = "Enclave: signing failed – ${ex.message}"
            JOptionPane.showMessageDialog(
                this,
                "Signing failed: ${ex.message}",
                "Signing Error",
                JOptionPane.ERROR_MESSAGE
            )
        }
    }

    private fun exportAlnRow() {
        val signatureHex = signatureField.text.trim()
        if (signatureHex.isEmpty()) {
            JOptionPane.showMessageDialog(
                this,
                "No signature present. Sign the statement before exporting.",
                "Export Error",
                JOptionPane.ERROR_MESSAGE
            )
            return
        }

        val did = didField.text.trim()
        val stewardName = stewardNameField.text.trim()
        val region = regionField.text.trim()
        val ecoScoreText = ecoScoreField.text.trim()
        val carbonOffsetText = carbonOffsetField.text.trim()
        val notes = notesArea.text.trim()

        val ecoScore = ecoScoreText.toDoubleOrNull() ?: 0.0
        val carbonOffset = carbonOffsetText.toDoubleOrNull() ?: 0.0

        val alnRow = buildSignedAlnRow(
            stewardName = stewardName,
            did = did,
            region = region,
            ecoScore = ecoScore,
            carbonOffset = carbonOffset,
            notes = notes,
            signatureHex = signatureHex,
            signatureAlgo = signatureAlgoField.text.trim()
        )

        val chooser = JFileChooser()
        chooser.dialogTitle = "Export Signed StewardEcoWealthStatement2026v1"
        chooser.fileFilter = FileNameExtensionFilter("ALN spec row (*.aln)", "aln")
        chooser.selectedFile = File("StewardEcoWealthStatement2026v1_${did}.aln")

        val result = chooser.showSaveDialog(this)
        if (result == JFileChooser.APPROVE_OPTION) {
            val outFile = chooser.selectedFile
            try {
                outFile.writeText(alnRow, Charsets.UTF_8)
                JOptionPane.showMessageDialog(
                    this,
                    "Signed ALN row exported to: ${outFile.absolutePath}",
                    "Export Successful",
                    JOptionPane.INFORMATION_MESSAGE
                )
            } catch (ex: Exception) {
                JOptionPane.showMessageDialog(
                    this,
                    "Failed to write ALN row: ${ex.message}",
                    "Export Error",
                    JOptionPane.ERROR_MESSAGE
                )
            }
        }
    }

    private fun validateDidFormat(did: String): Boolean {
        // Simple DID validation anchored to Bostrom/ALN patterns.
        if (did.startsWith("bostrom") || did.startsWith("aln")) {
            return did.length in 40..80
        }
        if (did.startsWith("0x")) {
            return did.length == 42
        }
        if (did.startsWith("zeta")) {
            return did.length >= 40
        }
        return false
    }

    private fun buildStatementPayload(
        stewardName: String,
        did: String,
        region: String,
        ecoScore: Double,
        carbonOffset: Double,
        notes: String
    ): String {
        // Deterministic payload for signing (no trailing spaces, fixed field order).
        return buildString {
            append("StewardEcoWealthStatement2026v1|")
            append("steward_name=").append(stewardName).append("|")
            append("did=").append(did).append("|")
            append("region=").append(region).append("|")
            append("eco_wealth_score=").append(ecoScore).append("|")
            append("carbon_offset_tco2e=").append(carbonOffset).append("|")
            append("notes=").append(notes.replace("\n", "\\n"))
        }
    }

    private fun buildSignedAlnRow(
        stewardName: String,
        did: String,
        region: String,
        ecoScore: Double,
        carbonOffset: Double,
        notes: String,
        signatureHex: String,
        signatureAlgo: String
    ): String {
        // Single-row ALN entry ready for registry ingestion.
        // The registry can parse this row, associate the DID and signature, and
        // anchor it into the EcoWealth registry table.
        val sanitizedNotes = notes.replace("\n", "\\n")

        return """
row StewardEcoWealthStatement2026v1 {
  steward_name   = "$stewardName"
  did            = "$did"
  region         = "$region"
  eco_wealth_score = $ecoScore
  carbon_offset_tco2e = $carbonOffset
  notes          = "$sanitizedNotes"
  signature_algo = "$signatureAlgo"
  signature_hex  = "$signatureHex"
}
""".trimIndent()
    }
}

/**
 * Bridge object for calling into Rust signing enclave via JNI.
 *
 * The corresponding native implementation must be provided by
 * the Rust crate compiled as a shared library and exposing:
 *
 *  JNIEXPORT jstring JNICALL
 *  Java_org_prometheuspraxis_steward_RoHSigningBridge_signEcoWealthStatement(
 *      JNIEnv* env,
 *      jclass,
 *      jstring j_did,
 *      jstring j_payload
 *  );
 */
object RoHSigningBridge {

    @JvmStatic
    external fun signEcoWealthStatement(did: String, payload: String): String
}
