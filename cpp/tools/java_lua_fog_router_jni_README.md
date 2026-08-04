# Java↔Lua Telemetry Bridge via JNI and SHAP-Based Blast-Radius Explanations

## Java-to-Lua FOG Router Bridge (JNI + Lua C API)

- `cpp/tools/java_lua_fog_router_jni_bridge.cpp` implements a native JNI bridge:
  - A single embedded Lua VM (`lua_State* L`) is initialised and loads `lua/fog_router_predicates.lua`.
  - Java passes a serialized telemetry protobuf (`byte[]`) to `FogRouterBridge.route(...)`.
  - The JNI bridge:
    - Deserialises telemetry into viscous/turbid/organic fractions and canal capacity.
    - Calls Lua `FOGRouter.classify_media(...)`, `predicate_score(...)`, and `suggest_route(...)` directly using the Lua C API.
    - Returns the route string to Java.
  - Error handling:
    - Lua script load and function calls are checked; errors are logged and fall back to `"HOLD_TANK"`.
    - The Lua stack is carefully balanced, avoiding leaks or undefined behaviour.
  - Memory management:
    - Telemetry bytes are copied from Java into a `std::vector<unsigned char>` for safe deserialisation.
    - The Lua VM is reused across calls, reducing overhead compared to piping to an external process.

## SHAP Feature Importance Storage for Blast-Radius Alerts

- `kotlin/src/main/kotlin/org/cyboquatic/alerts/BlastRadiusShapStorage.kt` defines a data-science integration object:
  - Gradient-boosted trees models trained on `surcharge_events` features (antecedent moisture, canal age, surge magnitude, wall integrity, etc.) produce SHAP values per event.
  - Dominant SHAP features typically include:
    - `surge_magnitude`: strongest driver of predicted breach radius.
    - `antecedent_moisture` / `soil_moisture_deficit`: controls soil strength and seepage.
    - `wall_integrity_score` and `canal_age_years`: structural vulnerability.
  - SHAP values are stored in `surcharge_shap_values` with `(event_id, feature_name, shap_value)` and indexed for fast lookup.
  - The Kotlin alerting service can query `getTopFeaturesForEvent(eventId)` to obtain local explanations (e.g., “High surge magnitude and low wall integrity drove this breach risk”), enhancing interpretability of blast-radius predictions and guiding targeted eco-restoration responses.

Technical justification: The JNI/Lua bridge eliminates process pipe overhead and allows tight integration of Java telemetry handling with Lua FOG routing logic, suitable for real-time cyboquatic decisions. The SHAP storage schema and Kotlin utilities provide a robust mechanism to persist and query local feature attributions for blast-radius predictions, enabling governance-aware, explainable alerts in eco-restoration control systems.
