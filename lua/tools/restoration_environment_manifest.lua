-- Repository: mk-bluebird/Prometheus-Praxis
-- Filename: lua/tools/restoration_environment_manifest.lua
-- Destination: lua/tools

local restoration_environment_manifest = {}

local Manifest = {}
Manifest.__index = Manifest

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function copy_table(source)
    local result = {}

    for key, value in pairs(source) do
        if type(value) == "table" then
            result[key] = copy_table(value)
        else
            result[key] = value
        end
    end

    return result
end

local function assert_string(value, name)
    assert(type(value) == "string" and value ~= "",
        name .. " must be a non-empty string")
end

local function sorted_keys(values)
    local keys = {}

    for key in pairs(values) do
        keys[#keys + 1] = key
    end

    table.sort(keys)
    return keys
end

function Manifest.new(metadata)
    assert(type(metadata) == "table", "metadata must be a table")

    assert_string(metadata.project, "project")
    assert_string(metadata.environment_revision, "environment_revision")
    assert_string(metadata.lua_runtime, "lua_runtime")
    assert_string(metadata.luarocks_version, "luarocks_version")
    assert_string(metadata.teal_version, "teal_version")
    assert_string(metadata.h3_version, "h3_version")
    assert_string(metadata.native_abi, "native_abi")
    assert_string(metadata.platform, "platform")
    assert_string(metadata.source_revision, "source_revision")

    assert(type(metadata.dependencies) == "table",
        "dependencies must be a table")
    assert(type(metadata.datasets) == "table",
        "datasets must be a table")
    assert(type(metadata.build_flags) == "table",
        "build_flags must be a table")

    return setmetatable({
        metadata = copy_table(metadata),
    }, Manifest)
end

function Manifest:canonical_text()
    local metadata = self.metadata
    local lines = {
        "project=" .. metadata.project,
        "environment_revision=" .. metadata.environment_revision,
        "lua_runtime=" .. metadata.lua_runtime,
        "luarocks_version=" .. metadata.luarocks_version,
        "teal_version=" .. metadata.teal_version,
        "h3_version=" .. metadata.h3_version,
        "native_abi=" .. metadata.native_abi,
        "platform=" .. metadata.platform,
        "source_revision=" .. metadata.source_revision,
    }

    local sections = {
        dependencies = metadata.dependencies,
        datasets = metadata.datasets,
        build_flags = metadata.build_flags,
    }

    for section_name, section in pairs(sections) do
        local keys = sorted_keys(section)

        for index = 1, #keys do
            local key = keys[index]
            lines[#lines + 1] = section_name
                .. "." .. tostring(key)
                .. "=" .. tostring(section[key])
        end
    end

    table.sort(lines)
    return table.concat(lines, "\n")
end

function Manifest:validate_against(expected)
    assert(type(expected) == "table", "expected must be a table")

    local differences = {}

    for key, expected_value in pairs(expected) do
        local actual_value = self.metadata[key]

        if actual_value ~= expected_value then
            differences[#differences + 1] = {
                field = key,
                expected = expected_value,
                actual = actual_value,
            }
        end
    end

    return {
        matches = #differences == 0,
        differences = differences,
    }
end

function Manifest:reproducibility_requirements()
    return {
        "Pin the flake input graph and retain its lockfile with every released monitoring run.",
        "Record source revision, Lua runtime, LuaRocks, Teal, H3, compiler, linker, operating-system platform, and native ABI.",
        "Build from declared immutable inputs; prohibit implicit package downloads and unpinned system-library discovery.",
        "Preserve compiler flags and deterministic build settings used for native geospatial bindings.",
        "Version datasets, spatial reference definitions, H3 resolution conventions, model parameters, and input-schema revisions.",
        "Record command entry point, input manifest, generated output manifest, and validation-test result for each monitoring batch.",
        "Run deterministic regression and numerical cross-language tests when any runtime, compiler, dependency, or model changes.",
        "Retain archived environments or rebuildable source inputs for the full monitoring and audit retention period.",
    }
end

function Manifest:metrics(
    dependency_pin_coverage,
    data_provenance_coverage,
    build_replay_validation,
    environment_drift_risk
)
    assert(finite_number(dependency_pin_coverage)
        and dependency_pin_coverage >= 0.0
        and dependency_pin_coverage <= 1.0,
        "dependency_pin_coverage must be within [0, 1]")
    assert(finite_number(data_provenance_coverage)
        and data_provenance_coverage >= 0.0
        and data_provenance_coverage <= 1.0,
        "data_provenance_coverage must be within [0, 1]")
    assert(finite_number(build_replay_validation)
        and build_replay_validation >= 0.0
        and build_replay_validation <= 1.0,
        "build_replay_validation must be within [0, 1]")
    assert(finite_number(environment_drift_risk)
        and environment_drift_risk >= 0.0
        and environment_drift_risk <= 1.0,
        "environment_drift_risk must be within [0, 1]")

    local knowledge_factor = (
        dependency_pin_coverage
        * data_provenance_coverage
        * build_replay_validation
    ) ^ (1.0 / 3.0)

    return {
        knowledge_factor = knowledge_factor,
        eco_impact_value = knowledge_factor * (1.0 - environment_drift_risk),
        harm_risk = environment_drift_risk,
    }
end

function restoration_environment_manifest.new(metadata)
    return Manifest.new(metadata)
end

return restoration_environment_manifest
