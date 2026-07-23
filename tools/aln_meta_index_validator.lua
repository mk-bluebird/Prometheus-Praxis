-- File: tools/aln_meta_index_validator.lua
-- Destination: Prometheus-Praxis/tools/aln_meta_index_validator.lua
-- License: MIT OR Apache-2.0

local MetaIndexValidator = {}

MetaIndexValidator.meta = {
    knowledge_factor = 0.90,
    eco_impact_value = 0.94,
    notes = "Crawls aln/ tree, validates cross-references (e.g., planecontractid → PlaneWeightsShard2026v1), and emits a GitHub Actions summary."
}

----------------------------------------------------------------------
-- Configuration
----------------------------------------------------------------------

local CONFIG = {
    aln_root = "aln",
    plane_weights_file = "aln/PlaneWeightsShard2026v1.aln",
    github_summary_path = os.getenv("GITHUB_STEP_SUMMARY") or "aln_meta_index_summary.md"
}

----------------------------------------------------------------------
-- Filesystem utilities (portable, uses io.popen + system dir)
----------------------------------------------------------------------

local function list_aln_files(root)
    local files = {}

    local cmd
    if package.config:sub(1, 1) == "\\" then
        cmd = 'dir /b /s "' .. root .. '"'
    else
        cmd = 'find "' .. root .. '" -type f'
    end

    local p = io.popen(cmd)
    if not p then
        return files
    end

    for line in p:lines() do
        if line:match("%.aln$") then
            table.insert(files, line)
        end
    end
    p:close()

    return files
end

----------------------------------------------------------------------
-- Simple file read
----------------------------------------------------------------------

local function slurp(path)
    local f = io.open(path, "r")
    if not f then
        return nil
    end
    local content = f:read("*a")
    f:close()
    return content
end

----------------------------------------------------------------------
-- Parse PlaneWeightsShard2026v1 for valid planecontractids
--
-- Assumes rows like:
--   row PlaneWeightsShard2026v1 {
--     planecontractid = "N2915M"
--     ...
--   }
----------------------------------------------------------------------

local function collect_plane_contract_ids(path)
    local ids = {}

    local content = slurp(path)
    if not content then
        return ids
    end

    for row_block in content:gmatch("row%s+PlaneWeightsShard2026v1%s*{(.-)}") do
        local id = row_block:match('planecontractid%s*=%s*"(.-)"')
        if id and id ~= "" then
            ids[id] = true
        end
    end

    return ids
end

----------------------------------------------------------------------
-- Scan generic ALN files for planecontractid references
--
-- We treat any occurrence of planecontractid="..." as a reference
-- that must exist in PlaneWeightsShard2026v1.
----------------------------------------------------------------------

local function scan_file_for_plane_refs(path, valid_ids, issues)
    local content = slurp(path)
    if not content then
        table.insert(issues.missing_files, path)
        return
    end

    for ref in content:gmatch('planecontractid%s*=%s*"(.-)"') do
        if not valid_ids[ref] then
            table.insert(issues.invalid_plane_refs, {
                file = path,
                planecontractid = ref
            })
        else
            issues.valid_plane_refs_count = issues.valid_plane_refs_count + 1
        end
    end
end

----------------------------------------------------------------------
-- Main validation routine
----------------------------------------------------------------------

function MetaIndexValidator.run()
    local summary = {
        header = "# ALN Meta Index Validation – Always Improve\n",
        files_scanned = 0,
        missing_files = {},
        invalid_plane_refs = {},
        valid_plane_refs_count = 0
    }

    -- Step 1: collect authoritative planecontractids
    local valid_plane_ids = collect_plane_contract_ids(CONFIG.plane_weights_file)

    -- Step 2: crawl aln/ tree and validate references
    local aln_files = list_aln_files(CONFIG.aln_root)
    summary.files_scanned = #aln_files

    for _, path in ipairs(aln_files) do
        scan_file_for_plane_refs(path, valid_plane_ids, summary)
    end

    -- Step 3: build GitHub Actions summary markdown
    local lines = {}
    table.insert(lines, summary.header)
    table.insert(lines, "")
    table.insert(lines, string.format("- **ALN root**: `%s`", CONFIG.aln_root))
    table.insert(lines, string.format("- **Plane weights source**: `%s`", CONFIG.plane_weights_file))
    table.insert(lines, string.format("- **Files scanned**: `%d`", summary.files_scanned))
    table.insert(lines, string.format("- **Valid planecontractid references**: `%d`", summary.valid_plane_refs_count))
    table.insert(lines, "")

    if #summary.missing_files > 0 or #summary.invalid_plane_refs > 0 then
        table.insert(lines, "## Issues Detected")
        table.insert(lines, "")
    end

    if #summary.missing_files > 0 then
        table.insert(lines, "### Missing ALN files")
        table.insert(lines, "")
        for _, path in ipairs(summary.missing_files) do
            table.insert(lines, string.format("- `%s` (unreadable or missing)", path))
        end
        table.insert(lines, "")
    end

    if #summary.invalid_plane_refs > 0 then
        table.insert(lines, "### Invalid planecontractid cross-references")
        table.insert(lines, "")
        table.insert(lines, "| File | planecontractid |")
        table.insert(lines, "|------|-----------------|")
        for _, issue in ipairs(summary.invalid_plane_refs) do
            table.insert(lines, string.format("| `%s` | `%s` |", issue.file, issue.planecontractid))
        end
        table.insert(lines, "")
    end

    if #summary.invalid_plane_refs == 0 and #summary.missing_files == 0 then
        table.insert(lines, "## Status")
        table.insert(lines, "")
        table.insert(lines, "All `planecontractid` cross-references resolve to `PlaneWeightsShard2026v1`. ALN meta index is consistent for this shard set.")
    end

    local out = table.concat(lines, "\n")

    local ok, err
    local f = io.open(CONFIG.github_summary_path, "w")
    if f then
        f:write(out)
        f:close()
        ok = true
    else
        ok = false
        err = "Failed to open summary path: " .. tostring(CONFIG.github_summary_path)
    end

    return {
        success = ok,
        error = err,
        summary_markdown = out
    }
end

----------------------------------------------------------------------
-- CLI entrypoint for GitHub Actions
--
-- Usage (in CI job):
--   lua tools/aln_meta_index_validator.lua
--
-- The script will write to $GITHUB_STEP_SUMMARY if set; otherwise
-- it will write aln_meta_index_summary.md in the repo root.
----------------------------------------------------------------------

local function main()
    local result = MetaIndexValidator.run()

    if not result.success then
        io.stderr:write("ALN meta index validation failed: ", result.error or "unknown error", "\n")
        os.exit(1)
    end

    print("ALN meta index validation summary written to: " .. CONFIG.github_summary_path)
end

if pcall(debug.getinfo, 1, "S") then
    -- If executed as a script (not required/loaded), run main.
    local info = debug.getinfo(1, "S")
    if info and info.source == "@aln_meta_index_validator.lua" then
        main()
    end
end

return MetaIndexValidator
