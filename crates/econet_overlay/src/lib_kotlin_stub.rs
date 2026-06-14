// filename: crates/econet_overlay/src/lib_kotlin_stub.rs
// destination: ecorestoration_shard/crates/econet_overlay/src/lib_kotlin_stub.rs
//
// This file is illustrative Rust-side documentation for Kotlin/Lua overlays.
// It does not expose FFI and remains non-actuating.
//

/// Kotlin sealed result type mapping (documentation-only, no JNI binding here):
///
/// sealed class OverlayResult<out T> {
///     data class Ok<T>(val value: T) : OverlayResult<T>()
///     object NotFound : OverlayResult<Nothing>()
///     data class InvalidJson(val message: String) : OverlayResult<Nothing>()
///     data class BackendError(val message: String) : OverlayResult<Nothing>()
/// }
///
/// data class RepoSummary(
///     val filename: String,
///     val destination: String,
///     val repotarget: String,
///     val roleband: String,
///     val lanedefault: String,
///     val regionscope: String,
///     val planes: List<String>,
///     val logicalname: String,
///     val artifactkind: String,
///     val econscope: String,
///     val nonactuating: Boolean,
///     val kerbandk: Double,
///     val kerbande: Double,
///     val kerbandr: Double,
///     val authorbostrom: String,
/// )
///
/// fun RepoSummary.toChatSummary(): String {
///     return "Artifact $logicalname in repo $repotarget is a $artifactkind in role band " +
///         "$roleband for lane $lanedefault in region $regionscope. " +
///         "Its KER targets are K=%.2f, E=%.2f, R=%.2f, and it touches planes %s. ".format(
///             kerbandk, kerbande, kerbandr, planes.joinToString(prefix = "[", postfix = "]")
///         ) + "The artifact is marked non-actuating=$nonactuating and authored by $authorbostrom."
/// }
///
/// These mappings are intended to be generated from the econetfileindex and eco_idx master
/// index views via the Rust cdylib JSON envelopes, keeping Kotlin strictly read-only.
pub fn kotlin_overlay_stub_marker() -> &'static str {
    "econet_overlay_kotlin_stub_non_actuating"
}
