-- File: cyboquatic_microplastic_risk.lua
-- Destination: Prometheus-Praxis/tools/cyboquatic_microplastic_risk.lua
-- License: MIT OR Apache-2.0

local CyboquaticMicroplasticMaterialsRisk = {}

-- Knowledge factor and eco-impact scoring for this module
CyboquaticMicroplasticMaterialsRisk.meta = {
    knowledge_factor = 0.87,
    eco_impact_value = 0.93,
    notes = "Risk scoring and markdown reporting for cyboquatic microplastic materials, tuned for stewardship workflows in Prometheus-Praxis."
}

----------------------------------------------------------------------
-- Threshold schema
-- CyboquaticMicroplasticMaterialsRisk2026v1 is represented as a Lua table.
-- You can persist this in a separate config file and require() it;
-- here we define a default, overrideable baseline.
--
-- Each key is a metric name, with:
--   unit               : string
--   safe_max           : numeric upper bound for 'low' risk
--   caution_max        : numeric upper bound for 'medium' risk
--   critical_max       : numeric upper bound for 'high' risk (beyond this is extreme)
--   weight             : relative importance in composite risk score (0..1)
--   direction          : "higher-worse" or "lower-worse"
----------------------------------------------------------------------

CyboquaticMicroplasticMaterialsRisk.default_thresholds_2026v1 = {
    microplastic_ppm = {
        unit = "ppm",
        safe_max = 5.0,
        caution_max = 20.0,
        critical_max = 100.0,
        weight = 0.40,
        direction = "higher-worse"
    },
    particle_count_per_l = {
        unit = "particles/L",
        safe_max = 100.0,
        caution_max = 500.0,
        critical_max = 2000.0,
        weight = 0.25,
        direction = "higher-worse"
    },
    mean_particle_size_um = {
        unit = "µm",
        safe_max = 50.0,
        caution_max = 300.0,
        critical_max = 1000.0,
        weight = 0.15,
        direction = "higher-worse"
    },
    biofilm_index = {
        unit = "index",
        safe_max = 0.2,
        caution_max = 0.6,
        critical_max = 1.0,
        weight = 0.10,
        direction = "higher-worse"
    },
    trophic_transfer_index = {
        unit = "index",
        safe_max = 0.3,
        caution_max = 0.7,
        critical_max = 1.0,
        weight = 0.10,
        direction = "higher-worse"
    }
}

----------------------------------------------------------------------
-- Utility: clamp a value between 0 and 1
----------------------------------------------------------------------

local function clamp01(x)
    if x < 0.0 then
        return 0.0
    elseif x > 1.0 then
        return 1.0
    else
        return x
    end
end

----------------------------------------------------------------------
-- Utility: classify a metric value relative to thresholds.
-- Returns:
--   level : "low" | "medium" | "high" | "extreme"
--   normalized : 0.0 .. 1.0 (0 safe, 1 extreme)
--   description : short string for human readers
----------------------------------------------------------------------

local function classify_value(value, thr)
    if value == nil then
        return "unknown", 0.0, "no data"
    end

    local v = value
    local safe = thr.safe_max
    local caution = thr.caution_max
    local critical = thr.critical_max

    if thr.direction == "lower-worse" then
        -- Invert thresholds: here safe_max is the minimum safe value, etc.
        if v >= safe then
            return "low", 0.0, "within safe range"
        elseif v >= caution then
            return "medium", 0.33, "approaching concern"
        elseif v >= critical then
            return "high", 0.66, "acute concern"
        else
            return "extreme", 1.0, "critical deficit"
        end
    else
        -- Default: higher-worse
        if v <= safe then
            return "low", 0.0, "within safe range"
        elseif v <= caution then
            return "medium", 0.33, "elevated but manageable"
        elseif v <= critical then
            return "high", 0.66, "acute concern"
        else
            return "extreme", 1.0, "critical exceedance"
        end
    end
end

----------------------------------------------------------------------
-- Compute a composite risk score and per-metric classifications.
--
-- Inputs:
--   sensor_data : table { metric_name = numeric_value, ... }
--   thresholds  : table (same shape as default_thresholds_2026v1)
--
-- Output:
--   {
--     overall_score = 0.0 .. 1.0,
--     overall_band  = "low" | "medium" | "high" | "extreme",
--     metrics       = {
--       [metric_name] = {
--           value = <number or nil>,
--           unit = <string>,
--           level = <string>,
--           normalized = <number>,
--           description = <string>,
--           weight = <number>,
--       },
--     }
--   }
----------------------------------------------------------------------

function CyboquaticMicroplasticMaterialsRisk.compute_risk(sensor_data, thresholds)
    local thr = thresholds or CyboquaticMicroplasticMaterialsRisk.default_thresholds_2026v1
    local metrics = {}
    local weighted_sum = 0.0
    local weight_total = 0.0

    for metric_name, metric_thr in pairs(thr) do
        local v = sensor_data[metric_name]
        local level, normalized, description = classify_value(v, metric_thr)

        local weight = metric_thr.weight or 0.0
        weighted_sum = weighted_sum + normalized * weight
        weight_total = weight_total + weight

        metrics[metric_name] = {
            value = v,
            unit = metric_thr.unit,
            level = level,
            normalized = normalized,
            description = description,
            weight = weight
        }
    end

    if weight_total <= 0.0 then
        weight_total = 1.0
    end

    local overall_score = clamp01(weighted_sum / weight_total)
    local band
    if overall_score < 0.25 then
        band = "low"
    elseif overall_score < 0.5 then
        band = "medium"
    elseif overall_score < 0.75 then
        band = "high"
    else
        band = "extreme"
    end

    return {
        overall_score = overall_score,
        overall_band = band,
        metrics = metrics
    }
end

----------------------------------------------------------------------
-- Utility: simple formatting helpers
----------------------------------------------------------------------

local function fmt_float(x, decimals)
    if x == nil then
        return "n/a"
    end
    local d = decimals or 2
    local fmt = "%." .. tostring(d) .. "f"
    return string.format(fmt, x)
end

local function escape_markdown(text)
    if text == nil then
        return ""
    end
    -- Basic escaping to avoid breaking table pipes
    local s = tostring(text)
    s = s:gsub("|", "\\|")
    return s
end

----------------------------------------------------------------------
-- GitHub Markdown summary generator for weekly stewards' reports.
--
-- Inputs:
--   site_id      : string identifier for the cyboquatic site
--   week_label   : human-readable week (e.g. "2026‑W29")
--   risk_result  : table from compute_risk()
--   options      : table (optional)
--       {
--         include_raw_section = boolean (default true),
--         eco_notes           = string (optional narrative),
--         steward_name        = string,
--         bostrom_address     = string (for hex-stamp anchoring),
--       }
--
-- Output:
--   markdown : multi-line string ready for GitHub issues, PRs, or logs
----------------------------------------------------------------------

function CyboquaticMicroplasticMaterialsRisk.generate_markdown(site_id, week_label, risk_result, options)
    local opts = options or {}
    local include_raw = (opts.include_raw_section ~= false)
    local eco_notes = opts.eco_notes or ""
    local steward_name = opts.steward_name or "unassigned"
    local bostrom_address = opts.bostrom_address or "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"

    local lines = {}

    table.insert(lines, "# Cyboquatic Microplastic Materials Risk – Weekly Steward Report")
    table.insert(lines, "")
    table.insert(lines, string.format("- **Site**: `%s`", escape_markdown(site_id)))
    table.insert(lines, string.format("- **Week**: `%s`", escape_markdown(week_label)))
    table.insert(lines, string.format("- **Overall Risk Band**: **%s**", escape_markdown(risk_result.overall_band)))
    table.insert(lines, string.format("- **Composite Risk Score**: `%.3f` (0.0 safe → 1.0 extreme)", risk_result.overall_score))
    table.insert(lines, string.format("- **Steward**: `%s`", escape_markdown(steward_name)))
    table.insert(lines, string.format("- **Bostrom / ALN Anchor**: `%s`", escape_markdown(bostrom_address)))
    table.insert(lines, "")

    table.insert(lines, "## Metric Bands")
    table.insert(lines, "")
    table.insert(lines, "| Metric | Value | Unit | Level | Weighted Normalized | Narrative |")
    table.insert(lines, "|--------|-------|------|-------|---------------------|-----------|")

    for metric_name, m in pairs(risk_result.metrics) do
        local metric_label = escape_markdown(metric_name)
        local value_str = fmt_float(m.value, 2)
        local row = string.format(
            "| %s | %s | %s | %s | %.3f × %.2f | %s |",
            metric_label,
            value_str,
            escape_markdown(m.unit),
            escape_markdown(m.level),
            m.normalized,
            m.weight,
            escape_markdown(m.description)
        )
        table.insert(lines, row)
    end

    if include_raw then
        table.insert(lines, "")
        table.insert(lines, "## Raw Sensor Snapshot")
        table.insert(lines, "")
        table.insert(lines, "```json")
        -- Emit a minimal JSON-style block for traceability
        table.insert(lines, string.format('{"site_id":"%s","week":"%s","overall_score":%.3f,"overall_band":"%s",',
            escape_markdown(site_id),
            escape_markdown(week_label),
            risk_result.overall_score,
            escape_markdown(risk_result.overall_band)
        ))
        table.insert(lines, ' "metrics": {')
        local first = true
        for metric_name, m in pairs(risk_result.metrics) do
            if not first then
                table.insert(lines, ",")
            end
            first = false
            local fragment = string.format(
                '  "%s": {"value":%s,"unit":"%s","level":"%s","normalized":%.3f,"weight":%.3f}',
                metric_name,
                m.value ~= nil and fmt_float(m.value, 4) or "null",
                m.unit,
                m.level,
                m.normalized,
                m.weight
            )
            table.insert(lines, fragment)
        end
        table.insert(lines, " }")
        table.insert(lines, "}")
        table.insert(lines, "```")
    end

    if eco_notes ~= "" then
        table.insert(lines, "")
        table.insert(lines, "## Steward Eco‑Notes")
        table.insert(lines, "")
        table.insert(lines, eco_notes)
    end

    table.insert(lines, "")
    table.insert(lines, "## Hex‑Stamp Traceability")
    table.insert(lines, "")
    table.insert(lines, "- Schema: `CyboquaticMicroplasticMaterialsRisk2026v1`")
    table.insert(lines, "- Risk module: `cyboquatic_microplastic_risk.lua`")
    table.insert(lines, "- Identity: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`")
    table.insert(lines, "")
    table.insert(lines, "> Auto‑generated via Prometheus‑Praxis cyboquatic stewardship tooling. Changes to thresholds or scoring must be hex‑stamped and reviewed.")

    return table.concat(lines, "\n")
end

----------------------------------------------------------------------
-- Example usage (can be removed in production):
-- This block demonstrates how to wire sensor data into the module.
----------------------------------------------------------------------

function CyboquaticMicroplasticMaterialsRisk.example()
    local sensor_data = {
        microplastic_ppm = 12.7,
        particle_count_per_l = 420.0,
        mean_particle_size_um = 220.0,
        biofilm_index = 0.5,
        trophic_transfer_index = 0.65
    }

    local risk = CyboquaticMicroplasticMaterialsRisk.compute_risk(sensor_data, nil)
    local markdown = CyboquaticMicroplasticMaterialsRisk.generate_markdown(
        "cyboquatic‑cell‑AZ‑PHX‑001",
        "2026‑W30",
        risk,
        {
            steward_name = "Phoenix Steward Cell A",
            eco_notes = "Persistent medium-band risk driven by elevated particle counts and trophic transfer. Recommend filtration tuning and substrate audits.",
            bostrom_address = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"
        }
    )

    return markdown, risk
end

return CyboquaticMicroplasticMaterialsRisk
