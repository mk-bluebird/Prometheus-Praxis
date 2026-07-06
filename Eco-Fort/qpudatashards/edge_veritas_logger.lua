-- File: Eco-Fort/qpudatashards/edge_veritas_logger.lua
-- DirClass: QPUDATASHARD

local ffi = require("ffi")
local sqlite3 = require("lsqlite3")

-- Load Rust FFI
ffi.cdef[[
    char* econet_compute_hextrace(const char* taskid, int allowed, const char* reasons);
    void econet_free_string(char* ptr);
]]
local rust_lib = ffi.load("econet_decision_core")

local function log_edge_decision(db, task_id, allowed, reasons_json, ker_vector)
    local decision_id = "dec_" .. os.time() .. "_" .. math.random(1000)
    local timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ")
    
    -- 1. Compute Hextrace via Rust FFI
    local c_task = ffi.new("char[?]", #task_id + 1)
    ffi.copy(c_task, task_id)
    local c_reasons = ffi.new("char[?]", #reasons_json + 1)
    ffi.copy(c_reasons, reasons_json)
    
    local hex_ptr = rust_lib.econet_compute_hextrace(c_task, allowed and 1 or 0, c_reasons)
    local hex_trace = ffi.string(hex_ptr)
    rust_lib.econet_free_string(hex_ptr)
    
    -- 2. Prepare ALN Envelope (Simulated Veritas Chain Append)
    local veritas_ref = "veritas://chain/" .. hex_trace
    
    -- 3. Register in Local SQLite Spine (Non-Actuating)
    local stmt = db:prepare([[
        INSERT INTO decision_log_shard 
        (decisionid, taskid, allowed, reasons, hextrace, timestamputc, k_ker, e_ker, r_ker, janus_veritas_ref, lyapunov_residual, tsafe_margin)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ]])
    
    stmt:bind_values(decision_id, task_id, allowed and 1 or 0, reasons_json, hex_trace, timestamp, 
                     ker_vector[1], ker_vector[2], ker_vector[3], veritas_ref, -0.05, 3600.0)
    stmt:step()
    stmt:finalize()
    
    return decision_id, veritas_ref
end
