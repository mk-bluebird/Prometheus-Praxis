// Filename: android/app/src/main/java/com/cyboquatic/ui/VaultScanActivity.kt
class VaultScanActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Use MLKit/ZXing to scan QR, then:
        // val nodeId = decodeQrContent(qrText)
        // start KerStatusActivity with nodeId
    }
}

class KerStatusActivity : ComponentActivity() {
    private val viewModel: KerViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val nodeId = intent.getStringExtra("nodeId") ?: return
        viewModel.loadShard(nodeId)

        setContent {
            val uiState by viewModel.uiState.collectAsState()
            KerStatusScreen(uiState) { nodeIdForLog ->
                // navigate to MaintenanceLogActivity
            }
        }
    }
}
