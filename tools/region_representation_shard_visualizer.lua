-- File: region_representation_shard_visualizer.lua
-- Destination: Prometheus-Praxis/tools/region_representation_shard_visualizer.lua
-- License: MIT OR Apache-2.0

local RegionRepresentationVisualizer = {}

RegionRepresentationVisualizer.meta = {
    schema_id = "RegionRepresentationShard2026v1",
    knowledge_factor = 0.86,
    eco_impact_value = 0.92,
    notes = "Lua SVG overlay generator for region coverage maps, tuned for GitHub README embedding via CI."
}

----------------------------------------------------------------------
-- Expected RegionRepresentationShard2026v1 row schema
--
-- Each row is a Lua table with fields:
--   id            : unique region identifier (string)
--   label         : human-readable name (string)
--   x             : logical X coordinate (0.0 .. 1.0)
--   y             : logical Y coordinate (0.0 .. 1.0)
--   width         : logical width (0.0 .. 1.0)
--   height        : logical height (0.0 .. 1.0)
--   coverage      : 0.0 .. 1.0 (fractional coverage)
--   corridor_tag  : string ecosafety corridor id
--
-- Coordinates are interpreted in a normalized viewport and scaled
-- into actual SVG pixels.
----------------------------------------------------------------------

----------------------------------------------------------------------
-- Color mapping helpers
----------------------------------------------------------------------

local function clamp01(v)
    if v < 0 then return 0 end
    if v > 1 then return 1 end
    return v
end

local function coverage_to_color(coverage)
    local c = clamp01(coverage or 0.0)
    -- Low coverage: green; mid: yellow; high: red.
    if c < 0.25 then
        return "#2ECC71"  -- low, eco-safe
    elseif c < 0.5 then
        return "#F1C40F"  -- moderate
    elseif c < 0.75 then
        return "#E67E22"  -- elevated
    else
        return "#E74C3C"  -- high
    end
end

local function corridor_to_stroke(corridor_tag)
    if corridor_tag == "LOW_ENERGY_BIOCOMPATIBLE" then
        return "#1D8348"
    elseif corridor_tag == "MODERATE_ENERGY_LAB_ONLY" then
        return "#5D6D7E"
    elseif corridor_tag == "MODERATE_ENERGY_STRUCTURAL" then
        return "#7D3C98"
    elseif corridor_tag == "HIGH_ENERGY_CONTAINED" then
        return "#943126"
    elseif corridor_tag == "HIGH_ENERGY_RF_EDGE" then
        return "#1B4F72"
    else
        return "#34495E"
    end
end

local function corridor_to_opacity(corridor_tag)
    if corridor_tag == "LOW_ENERGY_BIOCOMPATIBLE" then
        return 0.7
    elseif corridor_tag == "MODERATE_ENERGY_LAB_ONLY" then
        return 0.6
    elseif corridor_tag == "MODERATE_ENERGY_STRUCTURAL" then
        return 0.6
    elseif corridor_tag == "HIGH_ENERGY_CONTAINED" then
        return 0.5
    elseif corridor_tag == "HIGH_ENERGY_RF_EDGE" then
        return 0.5
    else
        return 0.6
    end
end

----------------------------------------------------------------------
-- Escape helpers
----------------------------------------------------------------------

local function escape_attr(text)
    if text == nil then
        return ""
    end
    local s = tostring(text)
    s = s:gsub("&", "&amp;")
    s = s:gsub("<", "&lt;")
    s = s:gsub(">", "&gt;")
    s = s:gsub("\"", "&quot;")
    return s
end

----------------------------------------------------------------------
-- Main SVG generation
--
-- Inputs:
--   regions         : array-like table of RegionRepresentationShard2026v1 rows
--   options         : table
--       {
--         width_px  = integer (default 800),
--         height_px = integer (default 480),
--         background = string color (default "#0B1B2B"),
--         title      = string (optional),
--         show_labels = boolean (default true),
--       }
--
-- Output:
--   svg_str : string, ready to write as .svg and embed in README
----------------------------------------------------------------------

function RegionRepresentationVisualizer.render_svg(regions, options)
    local opts = options or {}
    local width_px = opts.width_px or 800
    local height_px = opts.height_px or 480
    local background = opts.background or "#0B1B2B"
    local title = opts.title or "Region Representation Coverage – RegionRepresentationShard2026v1"
    local show_labels = (opts.show_labels ~= false)

    local buf = {}

    table.insert(buf, string.format(
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">',
        width_px, height_px, width_px, height_px
    ))
    table.insert(buf, string.format('<title>%s</title>', escape_attr(title)))
    table.insert(buf, string.format('<rect x="0" y="0" width="%d" height="%d" fill="%s"/>',
        width_px, height_px, background))

    -- Legend bar
    table.insert(buf, '<g id="legend" font-family="system-ui, -apple-system, BlinkMacSystemFont, \'Segoe UI\', sans-serif" font-size="12" fill="#ECF0F1">')
    table.insert(buf, '<rect x="10" y="10" width="210" height="90" rx="8" ry="8" fill="#17202A" stroke="#566573" stroke-width="1"/>')
    table.insert(buf, '<text x="20" y="28">Region coverage</text>')
    table.insert(buf, '<rect x="20" y="38" width="20" height="10" fill="#2ECC71"/><text x="45" y="47">low (&lt;25%)</text>')
    table.insert(buf, '<rect x="20" y="53" width="20" height="10" fill="#F1C40F"/><text x="45" y="62">moderate (25–50%)</text>')
    table.insert(buf, '<rect x="20" y="68" width="20" height="10" fill="#E67E22"/><text x="45" y="77">elevated (50–75%)</text>')
    table.insert(buf, '<rect x="20" y="83" width="20" height="10" fill="#E74C3C"/><text x="45" y="92">high (&gt;75%)</text>')
    table.insert(buf, '</g>')

    -- Regions
    table.insert(buf, '<g id="regions">')
    for _, region in ipairs(regions) do
        local rx = (region.x or 0) * width_px
        local ry = (region.y or 0) * height_px
        local rw = (region.width or 0.1) * width_px
        local rh = (region.height or 0.1) * height_px

        local coverage = region.coverage or 0.0
        local corridor_tag = region.corridor_tag or "UNKNOWN"

        local fill = coverage_to_color(coverage)
        local stroke = corridor_to_stroke(corridor_tag)
        local opacity = corridor_to_opacity(corridor_tag)

        local id = escape_attr(region.id or "")
        local label = escape_attr(region.label or id)

        table.insert(buf, string.format(
            '<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="%s" fill-opacity="%.2f" stroke="%s" stroke-width="1.5"/>',
            rx, ry, rw, rh, fill, opacity, stroke
        ))

        if show_labels and label ~= "" then
            local tx = rx + rw / 2
            local ty = ry + rh / 2
            table.insert(buf, string.format(
                '<text x="%.2f" y="%.2f" text-anchor="middle" dominant-baseline="central" fill="#ECF0F1">%s</text>',
                tx, ty, label
            ))
        end

        -- Tooltip-like metadata using <title> for screen-readers
        table.insert(buf, string.format(
            '<title>Region %s: coverage=%.2f corridor=%s</title>',
            id, coverage, escape_attr(corridor_tag)
        ))
    end
    table.insert(buf, '</g>')
    table.insert(buf, '</svg>')

    return table.concat(buf, "\n")
end

----------------------------------------------------------------------
-- Example usage (for local testing; can be removed in CI):
----------------------------------------------------------------------

function RegionRepresentationVisualizer.example()
    local regions = {
        {
            id = "az_phx_core",
            label = "Phoenix Core",
            x = 0.10, y = 0.20,
            width = 0.25, height = 0.30,
            coverage = 0.65,
            corridor_tag = "LOW_ENERGY_BIOCOMPATIBLE"
        },
        {
            id = "az_phx_industrial",
            label = "Industrial Edge",
            x = 0.45, y = 0.25,
            width = 0.30, height = 0.35,
            coverage = 0.80,
            corridor_tag = "HIGH_ENERGY_CONTAINED"
        },
        {
            id = "az_phx_rf_ring",
            label = "RF Ring",
            x = 0.20, y = 0.65,
            width = 0.50, height = 0.25,
            coverage = 0.40,
            corridor_tag = "HIGH_ENERGY_RF_EDGE"
        }
    }

    local svg = RegionRepresentationVisualizer.render_svg(regions, {
        width_px = 960,
        height_px = 540,
        title = "Prometheus-Praxis Region Representation Coverage (Phoenix AZ)",
        show_labels = true
    })

    return svg
end

return RegionRepresentationVisualizer
