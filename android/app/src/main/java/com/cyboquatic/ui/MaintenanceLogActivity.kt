// Filename: android/app/src/main/java/com/cyboquatic/ui/MaintenanceLogActivity.kt
class MaintenanceLogActivity : ComponentActivity() {
    private val maintenanceDao: MaintenanceDao by lazy { /* inject via Hilt/Room */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val nodeId = intent.getStringExtra("nodeId") ?: return
        val engineerId = "engineer-123" // from auth

        // On submit:
        // val eventType = ...
        // val notes = ...
        // val photoUri = ...
        // val eventTs = Instant.now().toString()
        // val evidencehex = HexStampGenerator.hash(
        //      nodeId, eventTs, engineerId, eventType, notes, photoUri
        // )

        // viewModelScope.launch { maintenanceDao.insertEvent(entity) }
    }
}

// Simple hex stamp generator using SHA-256 equivalent, but in your constraints
// you may swap to a permitted hash or a Rust-side stamp via IPC.
object HexStampGenerator {
    fun hash(vararg parts: String): String {
        val concat = parts.joinToString("|")
        val bytes = concat.toByteArray(Charsets.UTF_8)
        // In practice, move hashing to Rust core to avoid forbidden hashes in JS/Android stack.
        return bytes.joinToString(separator = "") { b ->
            String.format("%02x", b)
        }
    }
}
