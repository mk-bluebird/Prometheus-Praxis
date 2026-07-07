// Filename: android/app/src/main/java/com/cyboquatic/ui/KerViewModel.kt
class KerViewModel(
    private val shardDao: ShardDao,
) : ViewModel() {

    data class KerUiState(
        val nodeId: String = "",
        val kerK: Double = 0.0,
        val kerE: Double = 0.0,
        val kerR: Double = 0.0,
        val vt: Double = 0.0,
        val corridorSummary: String = "",
        val loading: Boolean = false,
        val error: String? = null,
    )

    private val _uiState = MutableStateFlow(KerUiState())
    val uiState: StateFlow<KerUiState> = _uiState

    fun loadShard(nodeId: String) {
        viewModelScope.launch {
            _uiState.value = _uiState.value.copy(loading = true, nodeId = nodeId)
            try {
                val shard = shardDao.latestShardForNode(nodeId)
                if (shard == null) {
                    _uiState.value = _uiState.value.copy(
                        loading = false,
                        error = "No shard found for node $nodeId",
                    )
                } else {
                    _uiState.value = KerUiState(
                        nodeId = shard.nodeId,
                        kerK = shard.kerK,
                        kerE = shard.kerE,
                        kerR = shard.kerR,
                        vt = shard.vt,
                        corridorSummary = shard.corridorStatus,
                        loading = false,
                        error = null,
                    )
                }
            } catch (e: Exception) {
                _uiState.value = _uiState.value.copy(
                    loading = false,
                    error = e.message,
                )
            }
        }
    }
}
