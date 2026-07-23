-- File: tools/prometheus_shard_layout_plantuml.lua
-- Destination: Prometheus-Praxis/tools/prometheus_shard_layout_plantuml.lua
-- License: MIT OR Apache-2.0

local LAYOUT_ALN = "aln/prometheus-shard-layout.v1.aln"
local ARCH_MD    = "ARCHITECTURE.md"

local function slurp(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local c = f:read("*a")
    f:close()
    return c
end

local function write_file(path, content)
    local f = io.open(path, "w")
    if not f then
        error("failed to open file for writing: " .. path)
    end
    f:write(content)
    f:close()
end

local function parse_shards(aln)
    local shards = {}

    for block in aln:gmatch("row%s+ShardNode%s*{(.-)}") do
        local shard_id = block:match('shard_id%s*=%s*"(.-)"') or ""
        local zone     = block:match('zone%s*=%s*"(.-)"') or ""
        local role     = block:match('role%s*=%s*"(.-)"') or ""
        local depends  = {}

        local deps_block = block:match("depends_on%s*=%s*%[(.-)%]")
        if deps_block then
            for dep in deps_block:gmatch('"(.-)"') do
                table.insert(depends, dep)
            end
        end

        table.insert(shards, {
            shard_id = shard_id,
            zone = zone,
            role = role,
            depends_on = depends
        })
    end

    return shards
end

local function generate_plantuml(shards)
    local lines = {}
    table.insert(lines, "@startuml")
    table.insert(lines, "title Prometheus Shard Layout 2026v1")

    -- components
    for _, s in ipairs(shards) do
        local label = string.format("%s\\n[%s, %s]", s.shard_id, s.zone, s.role)
        table.insert(lines, string.format("component \"%s\" as %s", label, s.shard_id))
    end

    -- dependencies
    for _, s in ipairs(shards) do
        for _, dep in ipairs(s.depends_on) do
            table.insert(lines, string.format("%s --> %s", s.shard_id, dep))
        end
    end

    table.insert(lines, "@enduml")
    return table.concat(lines, "\n")
end

local function update_architecture_md(plantuml_block)
    local existing = slurp(ARCH_MD) or ""
    local start_marker = "<!-- PROMETHEUS_SHARD_LAYOUT_START -->"
    local end_marker   = "<!-- PROMETHEUS_SHARD_LAYOUT_END -->"

    local block = {}
    table.insert(block, start_marker)
    table.insert(block, "")
    table.insert(block, "```plantuml")
    table.insert(block, plantuml_block)
    table.insert(block, "```")
    table.insert(block, "")
    table.insert(block, end_marker)
    local replacement = table.concat(block, "\n")

    if existing:find(start_marker, 1, true) and existing:find(end_marker, 1, true) then
        local before = existing:match("^(.-)" .. start_marker) or ""
        local after  = existing:match(end_marker .. "(.*)$") or ""
        local new_md = before .. replacement .. after
        write_file(ARCH_MD, new_md)
    else
        local new_md = existing
        if #existing > 0 and not existing:match("\n$") then
            new_md = new_md .. "\n"
        end
        new_md = new_md .. "\n\n" .. replacement .. "\n"
        write_file(ARCH_MD, new_md)
    end
end

local function main()
    local aln = slurp(LAYOUT_ALN)
    if not aln then
        io.stderr:write("Failed to read shard layout ALN: ", LAYOUT_ALN, "\n")
        os.exit(1)
    end

    local shards = parse_shards(aln)
    local plantuml = generate_plantuml(shards)
    update_architecture_md(plantuml)

    print("Prometheus shard layout PlantUML embedded into " .. ARCH_MD)
end

main()
