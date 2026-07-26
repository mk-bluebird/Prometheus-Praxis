-- File: ecorestorationshard/tools/aln_linter_sidecar/ker_report_client.lua

local socket = require("socket.unix")
local json = require("cjson")

--[[
Domain: Lua client for KER consistency reports.

This module connects to the ALN linter sidecar Unix socket,
reads JSON‑encoded KER reports, and exposes a simple API
for filtering and reacting to inconsistencies.
]]--

local LINTER_SOCKET_PATH = "/tmp/aln_linter_sidecar.sock"

local function connect()
    local cli, err = socket()
    if not cli then
        error("Failed to create unix socket client: " .. tostring(err))
    end
    local ok, conn_err = cli:connect(LINTER_SOCKET_PATH)
    if not ok then
        error("Failed to connect to linter socket: " .. tostring(conn_err))
    end
    return cli
end

local function read_reports()
    local cli = connect()
    local reports = {}

    while true do
        local line, err = cli:receive("*l")
        if not line then
            -- EOF or error.
            break
        end
        local ok, decoded = pcall(json.decode, line)
        if ok and type(decoded) == "table" then
            table.insert(reports, decoded)
        end
    end

    cli:close()
    return reports
end

local function filter_inconsistent(reports)
    local bad = {}
    for _, rep in ipairs(reports) do
        if not rep.evidence_found or not rep.ker_match then
            table.insert(bad, rep)
        end
    end
    return bad
end

local function summarize(reports)
    local ok_count = 0
    local bad_count = 0

    for _, rep in ipairs(reports) do
        if rep.evidence_found and rep.ker_match then
            ok_count = ok_count + 1
        else
            bad_count = bad_count + 1
        end
    end

    return {
        total = #reports,
        consistent = ok_count,
        inconsistent = bad_count,
    }
end

local function main()
    local reports = read_reports()
    local summary = summarize(reports)
    local bad = filter_inconsistent(reports)

    io.stdout:write("Total reports: " .. summary.total .. "\n")
    io.stdout:write("Consistent:    " .. summary.consistent .. "\n")
    io.stdout:write("Inconsistent:  " .. summary.inconsistent .. "\n")

    if #bad > 0 then
        io.stdout:write("\nInconsistent shards:\n")
        for _, rep in ipairs(bad) do
            io.stdout:write(string.format(
                "  %s :: evidence_found=%s ker_match=%s message=%s\n",
                rep.path,
                tostring(rep.evidence_found),
                tostring(rep.ker_match),
                rep.message
            ))
        end
    end
end

main()
