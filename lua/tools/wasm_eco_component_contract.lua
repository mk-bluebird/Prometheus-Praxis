-- Repository: mk-bluebird/Prometheus-Praxis
-- Filename: lua/tools/wasm_eco_component_contract.lua
-- Destination: lua/tools

local wasm_eco_component_contract = {}

local ComponentContract = {}
ComponentContract.__index = ComponentContract

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

local function validate_nonempty_string(value, name)
    assert(type(value) == "string" and value ~= "",
        name .. " must be a non-empty string")
end

local function validate_scalar(value, schema, name)
    if schema == "string" then
        validate_nonempty_string(value, name)
        return
    end

    if schema == "number" then
        assert(finite_number(value), name .. " must be finite")
        return
    end

    if schema == "boolean" then
        assert(type(value) == "boolean", name .. " must be boolean")
        return
    end

    error("unsupported scalar schema: " .. tostring(schema))
end

function ComponentContract.new()
    return setmetatable({
        interfaces = {},
    }, ComponentContract)
end

function ComponentContract:add_interface(name, input_schema, output_schema)
    validate_nonempty_string(name, "interface name")
    assert(self.interfaces[name] == nil, "interface already exists: " .. name)
    assert(type(input_schema) == "table", "input_schema must be a table")
    assert(type(output_schema) == "table", "output_schema must be a table")

    self.interfaces[name] = {
        input_schema = copy_table(input_schema),
        output_schema = copy_table(output_schema),
    }
end

function ComponentContract:validate_record(record, schema, direction)
    assert(type(record) == "table", direction .. " record must be a table")

    for field, field_type in pairs(schema) do
        assert(record[field] ~= nil,
            direction .. " record missing required field: " .. field)
        validate_scalar(record[field], field_type, direction .. "." .. field)
    end

    return true
end

function ComponentContract:validate_call(interface_name, input_record)
    local interface = self.interfaces[interface_name]
    assert(interface, "unknown component interface: " .. tostring(interface_name))

    return self:validate_record(
        input_record,
        interface.input_schema,
        "input"
    )
end

function ComponentContract:validate_result(interface_name, output_record)
    local interface = self.interfaces[interface_name]
    assert(interface, "unknown component interface: " .. tostring(interface_name))

    return self:validate_record(
        output_record,
        interface.output_schema,
        "output"
    )
end

function ComponentContract:dashboard_contract()
    return {
        component_model = "WebAssembly Component Model",
        host_languages = {
            "Lua for deterministic local scoring and validation",
            "Rust for compiled component adapters and browser-safe execution",
            "WebAssembly for portable browser deployment",
            "WIT for typed interface definitions",
        },
        boundary_types = {
            "string: H3 cell index, ISO-8601 timestamp, metric identifier, methodology version",
            "number: temperatures, fractions, areas, uncertainty, K/E/R values",
            "boolean: validation and scenario flags",
            "record: heat observations, aggregated H3 summaries, model parameters, decision-support metrics",
            "list<record>: time series, cell summaries, validation issues, scenario alternatives",
            "option<T>: unavailable sensor values or optional uncertainty bounds",
            "result<T, error-record>: recoverable validation and model errors",
        },
        required_interfaces = {
            "validate-heat-observation",
            "aggregate-h3-heat-summary",
            "score-restoration-scenario",
            "simulate-thermal-scenario",
            "export-dashboard-summary",
        },
        boundary_rule = "Transfer inert validated values only. Do not expose automatic network access, browser navigation, credentials, untrusted code evaluation, or state-changing actions through the component interface.",
    }
end

function ComponentContract:metrics(
    interface_test_coverage,
    schema_validation_quality,
    browser_efficiency_benefit,
    integration_or_data_exposure_risk
)
    assert(finite_number(interface_test_coverage)
        and interface_test_coverage >= 0.0
        and interface_test_coverage <= 1.0,
        "interface_test_coverage must be within [0, 1]")
    assert(finite_number(schema_validation_quality)
        and schema_validation_quality >= 0.0
        and schema_validation_quality <= 1.0,
        "schema_validation_quality must be within [0, 1]")
    assert(finite_number(browser_efficiency_benefit)
        and browser_efficiency_benefit >= 0.0
        and browser_efficiency_benefit <= 1.0,
        "browser_efficiency_benefit must be within [0, 1]")
    assert(finite_number(integration_or_data_exposure_risk)
        and integration_or_data_exposure_risk >= 0.0
        and integration_or_data_exposure_risk <= 1.0,
        "integration_or_data_exposure_risk must be within [0, 1]")

    local knowledge_factor = math.sqrt(
        interface_test_coverage * schema_validation_quality
    )

    return {
        knowledge_factor = knowledge_factor,
        eco_impact_value = knowledge_factor
            * browser_efficiency_benefit
            * (1.0 - integration_or_data_exposure_risk),
        harm_risk = integration_or_data_exposure_risk,
    }
end

function wasm_eco_component_contract.new()
    return ComponentContract.new()
end

return wasm_eco_component_contract
