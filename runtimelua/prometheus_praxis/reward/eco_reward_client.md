# Lua, Kotlin, and MCP usage patterns
Lua (via LuaJIT FFI)
Module example:

runtimelua/prometheus_praxis/reward/eco_reward_client.lua

Steps:

local ffi = require("ffi") and local json = require("cjson").

ffi.cdef:

     ```
      char* eco_reward_compute_window_json(
        const char* dbpath,
        const char* nodeid,
        const char* tstartutc,
        const char* tendutc,
        const char* rewardspec_json,
        const char* assets_json
      );
      void eco_reward_free_cstring(char* ptr);
      ```

  - 3. `local lib = ffi.load("ecorestoration_reward")`.  
  - 4. Build Lua tables for `RewardSpec` and `CredibleAsset`, encode to JSON.  
  - 5. Call the C function, turn the returned pointer into a Lua string, free via `eco_reward_free_cstring`, then `json.decode` to get the result.

### Kotlin/Android (via JNI)

- JNI binding:

  - Expose a native function:

    - `external fun computeRewardWindowJson(dbPath: String, nodeId: String, tStartUtc: String, tEndUtc: String, rewardSpecJson: String, assetsJson: String): String?`

  - JNI implementation calls the C ABI function, converts the C string to a Java/Kotlin `String`, and then frees it.

- Kotlin code can then parse the JSON into data classes and send to MCP/agent layers or UI.

### MCP agents

- Tool definition:

  - MCP tool: `"compute_nanoswarm_rewards"`:

    - Parameters:
      - `db_path` (string).  
      - `node_id` (string).  
      - `window_start_utc`, `window_end_utc` (string).  
      - `reward_spec` (structured JSON).  
      - `assets` (structured JSON).  

    - Implementation:

      - Calls into Lua or Kotlin clients above, which in turn invoke the C ABI and decode the JSON result.  

- The tool remains non‑actuating:

  - It only returns suggestions; further steps (ledger updates, payouts) must pass your governance gates and explicit consent workflows.

***

## 4. Direction for a research‑agent

For a research‑agent, this JSON FFI layer enables:[1]

- Cross‑language reward experimentation:

  - Quickly test different RewardSpec/asset configurations from Lua and Kotlin, using the same Rust kernel and SQLite eco spine.  

- Governance integration:

  - Combine reward outputs with:
    - Ecosafety corridors and Lyapunov residuals.  
    - Treaty and neurorights gate results.  
    - Data‑as‑labor policies.  
  - Then propose safe reward schemes that are eco‑restorative, neurorights‑aligned, and non‑actuating by default.  
