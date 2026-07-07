// Filename: android/app/src/main/java/com/cyboquatic/data/Shards.kt
@Entity(tableName = "qpudatashard")
data class QpuDataShardEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val nodeId: String,
    val windowStartTs: String,
    val windowEndTs: String,
    val kerK: Double,
    val kerE: Double,
    val kerR: Double,
    val vt: Double,
    val corridorStatus: String,
    val evidencehex: String,
)

@Entity(tableName = "maintenance_event")
data class MaintenanceEventEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val nodeId: String,
    val eventTs: String,
    val engineerId: String,
    val eventType: String,
    val notes: String?,
    val photoUri: String?,
    val evidencehex: String,
    val syncedToCore: Int = 0,
)

@Dao
interface ShardDao {
    @Query("SELECT * FROM qpudatashard WHERE nodeId = :nodeId ORDER BY windowEndTs DESC LIMIT 1")
    suspend fun latestShardForNode(nodeId: String): QpuDataShardEntity?
}

@Dao
interface MaintenanceDao {
    @Insert
    suspend fun insertEvent(event: MaintenanceEventEntity)

    @Query("SELECT * FROM maintenance_event WHERE syncedToCore = 0")
    suspend fun unsyncedEvents(): List<MaintenanceEventEntity>

    @Query("UPDATE maintenance_event SET syncedToCore = 1 WHERE id IN (:ids)")
    suspend fun markSynced(ids: List<Long>)
}
