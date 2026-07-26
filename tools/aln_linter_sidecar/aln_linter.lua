-- File: ecorestorationshard/tools/aln_linter_sidecar/aln_linter.lua

local lfs = require("lfs")
local json = require("cjson")
local sqlite3 = require("lsqlite3")
local socket = require("socket.unix")

--[[
Domain: Cross‑language ALN particle linter as a sidecar.

This Lua service:
  - Watches a repository path for .aln files.
  - Parses lightweight metadata (evidence_hex, KER header).
  - Cross‑checks evidence_hex against a SQLite registry.
  - Emits a "KER consistency" report over a Unix domain socket
    in Lua‑readable JSON, suitable for consumption by other Lua modules.

The intent is to mirror the Kotlin‑side logic in a language
that can be used directly in EcoNet diagnostics, while
maintaining simple, robust glue code.
]]--

local LINTER_SOCKET_PATH = "/tmp/aln_linter_sidecar.sock"
local POLL_INTERVAL_SEC = 5

local REGISTRY_DB_PATH =
    os.getenv("ALN_LINTER_REGISTRY_DB") or "ecorestoration_registry.sqlite"

local function log(msg)
    io.stderr:write("[aln_linter] " .. msg .. "\n")
end

local function open_registry_db(path)
    local db = sqlite3.open(path)
    if not db then
        error("Failed to open SQLite registry at " .. path)
    end
    return db
end

-- Simple .aln parser for top‑level metadata.
local function parse_aln_metadata(filepath)
    local fh, err = io.open(filepath, "r")
    if not fh then
        return nil, "Cannot open file: " .. err
    end

    local evidence_hex = nil
    local k_val, e_val, r_val = nil, nil, nil

    for line in fh:lines() do
        local trimmed = line:gsub("^%s+", ""):gsub("%s+$", "")
        if trimmed:match("^evidencehex") then
            evidence_hex = trimmed:match("evidencehex%s+([0-9A-Fa-fx]+)")
        elseif trimmed:match("^ker") then
            -- Example: ker 0.95 0.92 0.13
            local k, e, r = trimmed:match("ker%s+([0-9%.]+)%s+([0-9%.]+)%s+([0-9%.]+)")
            if k and e and r then
                k_val = tonumber(k)
                e_val = tonumber(e)
                r_val = tonumber(r)
            end
        end
    end

    fh:close()

    if not evidence_hex then
        return nil, "Missing evidence_hex in " .. filepath
    end

    return {
        path = filepath,
        evidence_hex = evidence_hex,
        k = k_val,
        e = e_val,
        r = r_val,
    }, nil
end

local function query_registry_for_evidence(db, evidence_hex)
    local stmt = db:prepare(
        "SELECT kerk, kere, kerr FROM regionalecoledgerparticles " ..
        "WHERE evidencehash = ? LIMIT 1"
    )
    if not stmt then
        return nil, "Failed to prepare registry query."
    end

    stmt:bind_values(evidence_hex)
    local row = stmt:step() == sqlite3.ROW and {
        k = stmt:get_value(0),
        e = stmt:get_value(1),
        r = stmt:get_value(2),
    } or nil

    stmt:finalize()
    return row, nil
end

local function compare_ker(aln_ker, registry_ker, epsilon)
    if not aln_ker.k or not registry_ker.k then
        return false
    end
    local function close(a, b)
        return math.abs(a - b) <= epsilon
    end
    return close(aln_ker.k, registry_ker.k)
        and close(aln_ker.e, registry_ker.e)
        and close(aln_ker.r, registry_ker.r)
end

local function build_report(meta, registry_row)
    local evidence_found = registry_row ~= nil

    local ker_match = false
    local message = ""

    if not evidence_found then
        ker_match = false
        message = "Evidence hex not found in registry."
    else
        if meta.k and meta.e and meta.r then
            ker_match = compare_ker(
                { k = meta.k, e = meta.e, r = meta.r },
                registry_row,
                1e-3
            )
            if ker_match then
                message = "KER aligned with registry evidence."
            else
                message = "KER mismatch between ALN and registry."
            end
        else
            ker_match = false
            message = "Missing KER triad in ALN shard."
        end
    end

    local report = {
        path = meta.path,
        evidence_hex = meta.evidence_hex,
        evidence_found = evidence_found,
        ker_match = ker_match,
        message = message,
        k = registry_row and registry_row.k or meta.k,
        e = registry_row and registry_row.e or meta.e,
        r = registry_row and registry_row.r or meta.r,
    }

    return report
end

local function create_unix_socket(path)
    -- Remove existing socket if any.
    os.remove(path)

    local server, err = socket()
    if not server then
        error("Failed to create unix socket: " .. tostring(err))
    end

    local ok, bind_err = server:bind(path)
    if not ok then
        error("Failed to bind unix socket: " .. tostring(bind_err))
    end

    local ok_listen, listen_err = server:listen()
    if not ok_listen then
        error("Failed to listen on unix socket: " .. tostring(listen_err))
    end

    log("Unix socket ready at " .. path)
    return server
end

local function list_aln_files(root_dir)
    local files = {}

    local function traverse(dir)
        for entry in lfs.dir(dir) do
            if entry ~= "." and entry ~= ".." then
                local full = dir .. "/" .. entry
                local attr = lfs.attributes(full)
                if attr.mode == "directory" then
                    traverse(full)
                elseif attr.mode == "file" and entry:match("%.aln$") then
                    table.insert(files, full)
                end
            end
        end
    end

    traverse(root_dir)
    return files
end

local function stream_reports(server, reports)
    -- Simple protocol:
    --  - Accept one client at a time.
    --  - Write JSON lines, then close.
    local client, err = server:accept()
    if not client then
        log("No client connected: " .. tostring(err))
        return
    end

    for _, rep in ipairs(reports) do
        local line = json.encode(rep) .. "\n"
        client:send(line)
    end

    client:close()
end

local function run_linter(root_dir)
    log("Starting ALN linter over directory: " .. root_dir)

    local db = open_registry_db(REGISTRY_DB_PATH)
    local server = create_unix_socket(LINTER_SOCKET_PATH)

    while true do
        local aln_files = list_aln_files(root_dir)
        local reports = {}

        for _, path in ipairs(aln_files) do
            local meta, perr = parse_aln_metadata(path)
            if not meta then
                log("Parse error for " .. path .. ": " .. perr)
            else
                local row, rerr = query_registry_for_evidence(db, meta.evidence_hex)
                if rerr then
                    log("Registry query error for " .. path .. ": " .. rerr)
                else
                    local report = build_report(meta, row)
                    table.insert(reports, report)
                end
            end
        end

        if #reports > 0 then
            stream_reports(server, reports)
        end

        socket.sleep(POLL_INTERVAL_SEC)
    end
end

local function main()
    local root = arg[1] or "."
    run_linter(root)
end

main()
