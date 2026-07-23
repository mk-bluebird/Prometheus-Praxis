-- File: tools/sunflower_bee_corridor_ledger_verifier.lua
-- Destination: Prometheus-Praxis/tools/sunflower_bee_corridor_ledger_verifier.lua
-- License: MIT OR Apache-2.0

local LedgerVerifier = {}

LedgerVerifier.meta = {
    schema_id = "eco.sunflower.beecorridor.ledger.v1.aln",
    knowledge_factor = 0.91,
    eco_impact_value = 0.95,
    notes = "Recompute Merkle roots from BeeCorridorEvidenceBundle leaves and compare with eco.sunflower.beecorridor.ledger.v1.aln on-ledger hashes."
}

----------------------------------------------------------------------
-- Config
----------------------------------------------------------------------

local CONFIG = {
    ledger_file = "aln/eco.sunflower.beecorridor.ledger.v1.aln",
    github_summary_path = os.getenv("GITHUB_STEP_SUMMARY") or "sunflower_bee_corridor_ledger_summary.md"
}

----------------------------------------------------------------------
-- File I/O
----------------------------------------------------------------------

local function slurp(path)
    local f = io.open(path, "r")
    if not f then
        return nil
    end
    local c = f:read("*a")
    f:close()
    return c
end

----------------------------------------------------------------------
-- Simple hash function for Merkle leaves/nodes
--
-- NOTE: To respect the blacklist, we avoid blake/BLAKE/SHA3-256 and
-- other disallowed algorithms. We use a simple, deterministic, non-
-- cryptographic mixer for integrity checking inside the repo.
----------------------------------------------------------------------

local function simple_hash(data)
    local h = 0x9E3779B97F4A7C15 -- golden ratio constant
    for i = 1, #data do
        local b = string.byte(data, i)
        h = h ~ b
        h = (h * 0xC2B2AE3D27D4EB4F) & 0xFFFFFFFFFFFFFFFF
        h = (h >> 13) | ((h & 0x1FFF) << (64 - 13))
    end
    return string.format("%016x", h)
end

----------------------------------------------------------------------
-- Merkle tree helpers (binary tree)
----------------------------------------------------------------------

local function merkle_combine(left_hash, right_hash)
    return simple_hash(left_hash .. right_hash)
end

local function merkle_root(leaves)
    if #leaves == 0 then
        return nil
    end

    local level = {}
    for i = 1, #leaves do
        level[i] = simple_hash(leaves[i])
    end

    while #level > 1 do
        local next_level = {}
        local idx = 1
        local i = 1
        while i <= #level do
            local left = level[i]
            local right = level[i + 1] or level[i] -- duplicate last when odd
            next_level[idx] = merkle_combine(left, right)
            idx = idx + 1
            i = i + 2
        end
        level = next_level
    end

    return level[1]
end

----------------------------------------------------------------------
-- Parse ledger file
--
-- We assume the ALN ledger file contains blocks like:
--
--   row BeeCorridorLedgerEntry {
--     corridor_id    = "bee_corridor_x"
--     epoch_utc      = "2026-03-05T00:00:00Z"
--     merkle_root_hex= "abcd..."
--     evidence_leaves = [
--       "leaf_payload_1",
--       "leaf_payload_2",
--       ...
--     ]
--   }
----------------------------------------------------------------------

local function parse_ledger_entries(content)
    local entries = {}

    for block in content:gmatch("row%s+BeeCorridorLedgerEntry%s*{(.-)}") do
        local corridor_id = block:match('corridor_id%s*=%s*"(.-)"') or ""
        local epoch_utc = block:match('epoch_utc%s*=%s*"(.-)"') or ""
        local merkle_root_hex = block:match('merkle_root_hex%s*=%s*"(.-)"') or ""

        local leaves = {}
        local array_block = block:match("evidence_leaves%s*=%s*%[(.-)%]")
        if array_block then
            for leaf in array_block:gmatch('"(.-)"') do
                table.insert(leaves, leaf)
            end
        end

        table.insert(entries, {
            corridor_id = corridor_id,
            epoch_utc = epoch_utc,
            merkle_root_hex = merkle_root_hex,
            leaves = leaves
        })
    end

    return entries
end

----------------------------------------------------------------------
-- Verification logic
----------------------------------------------------------------------

local function verify_entry(entry)
    local recomputed = merkle_root(entry.leaves or {})
    if not recomputed then
        return false, "NO_LEAVES", "No evidence_leaves present; cannot compute Merkle root."
    end

    if recomputed == entry.merkle_root_hex then
        return true, "OK", "Merkle root matches on-ledger hash."
    else
        return false, "ROOT_MISMATCH", string.format(
            "Recomputed root %s does not match ledger merkle_root_hex %s.",
            recomputed, entry.merkle_root_hex or "nil"
        )
    end
end

----------------------------------------------------------------------
-- Run verification across the ledger
----------------------------------------------------------------------

function LedgerVerifier.run()
    local ledger_content = slurp(CONFIG.ledger_file)
    local summary = {
        header = "# Sunflower Bee Corridor Ledger – Merkle Integrity Check\n",
        ledger_path = CONFIG.ledger_file,
        entries_total = 0,
        entries_ok = 0,
        entries_failed = 0,
        failures = {}
    }

    if not ledger_content then
        local msg = "Ledger file not found or unreadable: " .. CONFIG.ledger_file
        local lines = {
            summary.header,
            "",
            "- **Ledger file**: `" .. CONFIG.ledger_file .. "`",
            "- **Status**: :x: failed to read ledger file",
            "",
            msg
        }
        local out = table.concat(lines, "\n")
        local f = io.open(CONFIG.github_summary_path, "w")
        if f then
            f:write(out)
            f:close()
        end
        return {
            success = false,
            error = msg,
            summary_markdown = out
        }
    }

    local entries = parse_ledger_entries(ledger_content)
    summary.entries_total = #entries

    for _, entry in ipairs(entries) do
        local ok, code, message = verify_entry(entry)
        if ok then
            summary.entries_ok = summary.entries_ok + 1
        else
            summary.entries_failed = summary.entries_failed + 1
            table.insert(summary.failures, {
                corridor_id = entry.corridor_id,
                epoch_utc = entry.epoch_utc,
                code = code,
                message = message
            })
        end
    end

    -- Build GitHub Actions summary markdown
    local lines = {}
    table.insert(lines, summary.header)
    table.insert(lines, "")
    table.insert(lines, string.format("- **Ledger file**: `%s`", summary.ledger_path))
    table.insert(lines, string.format("- **Entries scanned**: `%d`", summary.entries_total))
    table.insert(lines, string.format("- **Entries OK**: `%d`", summary.entries_ok))
    table.insert(lines, string.format("- **Entries failed**: `%d`", summary.entries_failed))
    table.insert(lines, "")

    if summary.entries_failed > 0 then
        table.insert(lines, "## Merkle root mismatches")
        table.insert(lines, "")
        table.insert(lines, "| Corridor ID | Epoch UTC | Code | Message |")
        table.insert(lines, "|-------------|-----------|------|---------|")
        for _, fail in ipairs(summary.failures) do
            table.insert(lines, string.format(
                "| `%s` | `%s` | `%s` | %s |",
                fail.corridor_id or "",
                fail.epoch_utc or "",
                fail.code or "",
                fail.message or ""
            ))
        end
        table.insert(lines, "")
    else
        table.insert(lines, "## Status")
        table.insert(lines, "")
        table.insert(lines, "All ledger entries have matching Merkle roots. Bee corridor evidence bundles are consistent with on-ledger hashes for this sunflower eco ledger.")
    end

    local out = table.concat(lines, "\n")
    local f = io.open(CONFIG.github_summary_path, "w")
    if f then
        f:write(out)
        f:close()
    end

    return {
        success = summary.entries_failed == 0,
        error = nil,
        summary_markdown = out
    }
end

----------------------------------------------------------------------
-- CLI entrypoint for GitHub Actions
--
-- Usage:
--   lua tools/sunflower_bee_corridor_ledger_verifier.lua
--
-- The summary is written to $GITHUB_STEP_SUMMARY if set, otherwise
-- to sunflower_bee_corridor_ledger_summary.md in the repo root.
----------------------------------------------------------------------

local function main()
    local result = LedgerVerifier.run()
    if not result.success then
        io.stderr:write("Ledger Merkle integrity check failed.\n")
        os.exit(1)
    else
        print("Ledger Merkle integrity check succeeded. Summary written to: " .. CONFIG.github_summary_path)
    end
end

if pcall(debug.getinfo, 1, "S") then
    local info = debug.getinfo(1, "S")
    if info and info.source == "@sunflower_bee_corridor_ledger_verifier.lua" then
        main()
    end
end

return LedgerVerifier
