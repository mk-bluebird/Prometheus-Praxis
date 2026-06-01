// app/src/main/java/org/mkbluebird/cyberquatic/sync/AdaptiveSyncStrategy.kt
class AdaptiveSyncStrategy(
    private val api: CyboquaticAPI,
    private val db: KERDatabase,
    private val locationProvider: LocationProvider
) {
    companion object {
        const val SYNC_RADIUS_METERS = 1000.0
        const val BATTERY_THRESHOLD = 20 // Percent
    }
    
    suspend fun performAdaptiveSync(): SyncResult {
        val batteryLevel = getBatteryLevel()
        val location = locationProvider.getCurrentLocation()
        
        // Only sync if near known nodes AND battery sufficient
        if (batteryLevel < BATTERY_THRESHOLD) {
            return SyncResult.Skipped(reason = "Low battery")
        }
        
        val nearbyNodes = db.kerDao().getNodesWithin(
            lat = location.latitude,
            lon = location.longitude,
            radiusMeters = SYNC_RADIUS_METERS
        )
        
        if (nearbyNodes.isEmpty()) {
            return SyncResult.Skipped(reason = "No nodes nearby")
        }
        
        // Batch sync only nearby nodes
        val updates = nearbyNodes.map { node ->
            async { api.getKERScore(node.nodeId) }
        }.awaitAll()
        
        db.kerDao().insertAll(updates.mapNotNull { it.body() })
        
        return SyncResult.Success(updatedCount = updates.size)
    }
}
