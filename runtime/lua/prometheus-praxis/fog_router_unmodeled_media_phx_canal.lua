-- runtime/lua/prometheus-praxis/fog_router_unmodeled_media_phx_canal.lua
--
-- Prometheus-Praxis / FOG-router predicate module
-- Non-actuating Lua predicates over canal topology for "unmodeled media".
--
-- Purpose:
--   Provide a small, declarative predicate surface that AI agents and
--   Kotlin/Rust clients can consume to identify nodes in region
--   'PHX-CANAL' that are topologically exposed to unmodeled or stale
--   hydraulic/media segments within a bounded reachability radius.
--
--   This file is diagnostic-only. It never touches hardware and
--   never writes to any database or file. It only returns tables
--   that describe predicates in a compact, machine-readable form.

local M = {}

----------------------------------------------------------------------
-- Predicate shape
--
-- The compiled side (Kotlin/Rust) treats each predicate descriptor
-- as an AST with these fields:
--
--   type      : string  -- "within_region", "reachable_unmodeled",
--                       -- "AND", "OR", "NOT"
--   region    : string? -- region identifier, e.g. "PHX-CANAL"
--   max_depth : number? -- integer hop limit for reachability
--   left      : MediaPredicate? -- nested predicate
--   right     : MediaPredicate? -- nested predicate
--   inner     : MediaPredicate? -- for NOT
--
-- A "MediaPredicate" is simply such a descriptor table.
--
-- The graph parameter passed from Kotlin/Rust is not inspected
-- here; this module only outputs predicate descriptors.
----------------------------------------------------------------------

--- Construct a basic "reachable_unmodeled" predicate bound to a region.
-- @param region string region identifier (e.g. "PHX-CANAL")
-- @param max_depth integer maximum BFS depth in edges
-- @return table predicate descriptor
local function reachable_unmodeled(region, max_depth)
  return {
    type      = "reachable_unmodeled",
    region    = region,
    max_depth = max_depth,
  }
end

--- Construct a basic "within_region" predicate.
-- @param region string region identifier
-- @return table predicate descriptor
local function within_region(region)
  return {
    type   = "within_region",
    region = region,
  }
end

----------------------------------------------------------------------
-- Example 1:
--   Nodes in region 'PHX-CANAL' that can reach any unmodeled-media
--   edge within depth 3.
--
-- This variant returns a single "reachable_unmodeled" descriptor.
----------------------------------------------------------------------

--- Example factory: reachable-unmodeled in PHX-CANAL within depth 3.
-- The `graph` parameter is accepted for future extensibility but
-- not used here; the predicate is purely declarative.
-- @param graph any topology handle (ignored in this module)
-- @return table predicate descriptor
function M.phx_canal_reachable_unmodeled(graph)
  return reachable_unmodeled("PHX-CANAL", 3)
end

----------------------------------------------------------------------
-- Example 2:
--   Conjunctive predicate:
--     AND(
--       within_region('PHX-CANAL'),
--       reachable_unmodeled(max_depth = 3)
--     )
--
-- This is the form most Kotlin compilers will expect when they
-- separate region filters from graph reachability logic.
----------------------------------------------------------------------

--- Example factory: AND(within_region, reachable_unmodeled) for PHX-CANAL.
-- @param graph any topology handle (ignored in this module)
-- @return table predicate descriptor AST combining region and reachability.
function M.phx_canal_region_and_reachable_unmodeled(graph)
  return {
    type = "AND",
    left = within_region("PHX-CANAL"),
    right = reachable_unmodeled("PHX-CANAL", 3),
  }
end

----------------------------------------------------------------------
-- Generic helpers for agents
--
-- These helpers make the module more reusable without increasing
-- complexity for coding agents. They remain diagnostic-only.
----------------------------------------------------------------------

--- Build an AND of within_region(region) and reachable_unmodeled(region, max_depth).
-- @param region string region identifier
-- @param max_depth integer maximum graph depth
-- @return table predicate descriptor
function M.and_region_reachable_unmodeled(region, max_depth)
  return {
    type  = "AND",
    left  = within_region(region),
    right = reachable_unmodeled(region, max_depth),
  }
end

--- Build a plain reachable_unmodeled(region, max_depth) predicate.
-- @param region string region identifier
-- @param max_depth integer maximum graph depth
-- @return table predicate descriptor
function M.reachable_unmodeled(region, max_depth)
  return reachable_unmodeled(region, max_depth)
end

--- Build a plain within_region(region) predicate.
-- @param region string region identifier
-- @return table predicate descriptor
function M.within_region(region)
  return within_region(region)
end

----------------------------------------------------------------------
-- Module return
----------------------------------------------------------------------

return M
