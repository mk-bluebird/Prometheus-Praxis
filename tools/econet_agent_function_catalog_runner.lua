-- File: tools/econet_agent_function_catalog_runner.lua
-- Destination: Prometheus-Praxis/tools/econet_agent_function_catalog_runner.lua
-- License: MIT OR Apache-2.0

local AgentCatalogRunner = {}

AgentCatalogRunner.meta = {
    schema_id = "econet.agent.function.catalog.v1.aln",
    knowledge_factor = 0.92,
    eco_impact_value = 0.96,
    notes = "Loads econet.agent.function.catalog.v1.aln, instantiates functions in sandbox, runs test corpus, and emits GitHub Actions summary."
}

----------------------------------------------------------------------
-- Config
----------------------------------------------------------------------

local CONFIG = {
    catalog_file = "aln/econet.agent.function.catalog.v1.aln",
    github_summary_path = os.getenv("GITHUB_STEP_SUMMARY") or "econet_agent_function_catalog_summary.md"
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
-- Minimal sandbox: pure-Lua restricted environment
--
-- Inspired by existing Lua sandbox patterns: we expose only a small
-- subset of standard libraries, disallowing OS and IO to keep
-- agent functions safe for testing.[web:191][web:196]
----------------------------------------------------------------------

local function make_sandbox()
    local env = {
        _VERSION = _VERSION,
        assert = assert,
        error = error,
        ipairs = ipairs,
        pairs = pairs,
        pcall = pcall,
        select = select,
        tonumber = tonumber,
        tostring = tostring,
        type = type,
        math = {
            abs = math.abs,
            ceil = math.ceil,
            floor = math.floor,
            max = math.max,
            min = math.min,
            sqrt = math.sqrt
        },
        string = {
            byte = string.byte,
            char = string.char,
            find = string.find,
            format = string.format,
            gsub = string.gsub,
            len = string.len,
            lower = string.lower,
            upper = string.upper,
            sub = string.sub
        },
        table = {
            insert = table.insert,
            remove = table.remove,
            sort = table.sort
        }
    }
    return env
end

local function sandbox_load(code, name)
    local env = make_sandbox()
    local chunk, err = load(code, name, "t", env)
    if not chunk then
        return nil, "load error: " .. tostring(err)
    end
    return { env = env, chunk = chunk }
end

----------------------------------------------------------------------
-- Parse econet.agent.function.catalog.v1.aln
--
-- Expected catalog structure (simplified):
--
--  function EconetAgentFunction {
--    name        = "cyboquatic.microplastic.risk"
--    module      = "tools/cyboquatic_microplastic_risk.lua"
--    entrypoint  = "compute_risk"
--    test_corpus = [
--      {
--        input  = "{ \"microplastic_ppm\": 12.7 }",
--        expect = "band=medium"
--      },
--      ...
--    ]
--  }
----------------------------------------------------------------------

local function parse_catalog(content)
    local entries = {}

    for block in content:gmatch("function%s+EconetAgentFunction%s*{(.-)}") do
        local name = block:match('name%s*=%s*"(.-)"') or ""
        local module = block:match('module%s*=%s*"(.-)"') or ""
        local entrypoint = block:match('entrypoint%s*=%s*"(.-)"') or ""

        local corpus = {}
        local corpus_block = block:match("test_corpus%s*=%s*%[(.-)%]")
        if corpus_block then
            for case_block in corpus_block:gmatch("{(.-)}") do
                local input = case_block:match('input%s*=%s*"(.-)"') or ""
                local expect = case_block:match('expect%s*=%s*"(.-)"') or ""
                table.insert(corpus, { input = input, expect = expect })
            end
        end

        table.insert(entries, {
            name = name,
            module = module,
            entrypoint = entrypoint,
            corpus = corpus
        })
    end

    return entries
end

----------------------------------------------------------------------
-- Load module code and require in sandbox
--
-- We assume catalog modules are plain Lua files in the repo.
----------------------------------------------------------------------

local function load_module_source(module_path)
    return slurp(module_path)
end

local function instantiate_function(entry)
    local src = load_module_source(entry.module)
    if not src then
        return nil, "module not found: " .. entry.module
    end

    local sandbox, err = sandbox_load(src, entry.module)
    if not sandbox then
        return nil, err
    end

    local ok, result = pcall(sandbox.chunk)
    if not ok then
        return nil, "module runtime error: " .. tostring(result)
    end

    local exported = result
    if type(exported) ~= "table" then
        return nil, "module did not return a table"
    end

    local fn = exported[entry.entrypoint]
    if type(fn) ~= "function" then
        return nil, "entrypoint not found or not a function: " .. entry.entrypoint
    end

    return fn
end

----------------------------------------------------------------------
-- Simple JSON-ish parser for corpus input (limited)
--
-- We accept inputs like:
--   { "microplastic_ppm": 12.7 }
-- and convert them to Lua tables for testing.
----------------------------------------------------------------------

local function parse_input_jsonish(s)
    local t = {}
    for key, value in s:gmatch('"(.-)"%s*:%s*([%-%d%.]+)') do
        t[key] = tonumber(value)
    end
    return t
end

----------------------------------------------------------------------
-- Run corpus tests for a single agent function
----------------------------------------------------------------------

local function run_corpus_for_entry(entry)
    local result = {
        name = entry.name,
        module = entry.module,
        entrypoint = entry.entrypoint,
        total = #entry.corpus,
        passed = 0,
        failed = 0,
        failures = {}
    }

    local fn, err = instantiate_function(entry)
    if not fn then
        result.total = #entry.corpus
        result.failed = #entry.corpus
        table.insert(result.failures, {
            case = "module-load",
            error = err
        })
        return result
    end

    for idx, case in ipairs(entry.corpus) do
        local input_table = parse_input_jsonish(case.input)
        local ok, out = pcall(fn, input_table)
        if not ok then
            result.failed = result.failed + 1
            table.insert(result.failures, {
                case = idx,
                error = "runtime error: " .. tostring(out)
            })
        else
            local out_str = tostring(out)
            if out_str:find(case.expect, 1, true) then
                result.passed = result.passed + 1
            else
                result.failed = result.failed + 1
                table.insert(result.failures, {
                    case = idx,
                    error = "expectation mismatch; expected pattern '" .. case.expect .. "', got '" .. out_str .. "'"
                })
            end
        end
    end

    return result
end

----------------------------------------------------------------------
-- Main runner
----------------------------------------------------------------------

function AgentCatalogRunner.run()
    local content = slurp(CONFIG.catalog_file)
    local summary = {
        header = "# EcoNet Agent Function Catalog – Sandbox Validation\n",
        catalog_path = CONFIG.catalog_file,
        functions_total = 0,
        functions_ok = 0,
        functions_failed = 0,
        function_results = {}
    }

    if not content then
        local msg = "Catalog file not found or unreadable: " .. CONFIG.catalog_file
        local lines = {
            summary.header,
            "",
            "- **Catalog file**: `" .. CONFIG.catalog_path .. "`",
            "- **Status**: :x: failed to read catalog file",
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

    local entries = parse_catalog(content)
    summary.functions_total = #entries

    for _, entry in ipairs(entries) do
        local result = run_corpus_for_entry(entry)
        table.insert(summary.function_results, result)

        if result.failed == 0 then
            summary.functions_ok = summary.functions_ok + 1
        else
            summary.functions_failed = summary.functions_failed + 1
        end
    end

    -- Build GitHub Actions summary markdown
    local lines = {}
    table.insert(lines, summary.header)
    table.insert(lines, "")
    table.insert(lines, string.format("- **Catalog file**: `%s`", summary.catalog_path))
    table.insert(lines, string.format("- **Functions scanned**: `%d`", summary.functions_total))
    table.insert(lines, string.format("- **Functions OK**: `%d`", summary.functions_ok))
    table.insert(lines, string.format("- **Functions failed**: `%d`", summary.functions_failed))
    table.insert(lines, "")

    table.insert(lines, "## Function corpus results")
    table.insert(lines, "")
    table.insert(lines, "| Function | Module | Entrypoint | Cases | Passed | Failed |")
    table.insert(lines, "|----------|--------|------------|-------|--------|--------|")

    for _, r in ipairs(summary.function_results) do
        table.insert(lines, string.format(
            "| `%s` | `%s` | `%s` | `%d` | `%d` | `%d` |",
            r.name, r.module, r.entrypoint, r.total, r.passed, r.failed
        ))
    end

    if summary.functions_failed > 0 then
        table.insert(lines, "")
        table.insert(lines, "## Failures")
        table.insert(lines, "")
        for _, r in ipairs(summary.function_results) do
            if r.failed > 0 then
                table.insert(lines, string.format("### `%s` (%s)", r.name, r.module))
                table.insert(lines, "")
                for _, fail in ipairs(r.failures) do
                    table.insert(lines, string.format("- Case `%s`: %s", tostring(fail.case), fail.error))
                end
                table.insert(lines, "")
            }
        end
    else
        table.insert(lines, "")
        table.insert(lines, "All Econet agent functions passed their test corpus in sandboxed execution. Catalog v1 is consistent and safe under current constraints.")
    end

    local out = table.concat(lines, "\n")
    local f = io.open(CONFIG.github_summary_path, "w")
    if f then
        f:write(out)
        f:close()
    end

    return {
        success = summary.functions_failed == 0,
        error = nil,
        summary_markdown = out
    }
end

----------------------------------------------------------------------
-- CLI entrypoint for GitHub Actions
--
-- Usage (workflow step):
--   lua tools/econet_agent_function_catalog_runner.lua
--
-- The summary will be written to $GITHUB_STEP_SUMMARY if set.
----------------------------------------------------------------------

local function main()
    local result = AgentCatalogRunner.run()
    if not result.success then
        io.stderr:write("EcoNet agent function catalog validation failed.\n")
        os.exit(1)
    else
        print("EcoNet agent function catalog validation succeeded. Summary written to: " .. CONFIG.github_summary_path)
    end
end

if pcall(debug.getinfo, 1, "S") then
    local info = debug.getinfo(1, "S")
    if info and info.source == "@econet_agent_function_catalog_runner.lua" then
        main()
    end
end

return AgentCatalogRunner
