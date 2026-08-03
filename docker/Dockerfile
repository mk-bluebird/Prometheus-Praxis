# File: docker/Dockerfile
# Docker image for the Prometheus-Praxis governance stack:
# - C++ MCP server (universal toolbox + stdio bridge)
# - SQLite test database
# - Lua FOG router and governance daemons

FROM debian:stable-slim

RUN apt-get update && apt-get install -y \
    build-essential \
    sqlite3 \
    lua5.3 \
    lsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy C++ sources (MCP toolbox, stdio bridge, fog predictor, ALN/SQL generators, etc.)
COPY cpp /app/cpp
# Copy Lua governance scripts
COPY lua /app/lua

# Build C++ MCP toolbox and fog predictor
RUN g++ -std=c++20 -O2 -o mcp_stdio_governance_bridge cpp/tools/mcp_stdio_governance_bridge.cpp \
    && g++ -std=c++20 -O2 -o fog_schedule_predictor cpp/tools/fog_schedule_predictor.cpp

# Initialize SQLite test database
RUN sqlite3 prometheus_praxis.db "PRAGMA journal_mode=WAL;"

# Optionally load basic schema for hex_stability_carbon, schedule_cache, etc.
RUN sqlite3 prometheus_praxis.db "\
  CREATE TABLE IF NOT EXISTS hex_stability_carbon (\
    hex_id TEXT PRIMARY KEY,\
    band TEXT,\
    ker_k REAL,\
    ker_e REAL,\
    ker_r REAL,\
    carbon_intensity REAL,\
    max_carbon REAL,\
    ker_s REAL,\
    carbon_corridor REAL,\
    v_residual REAL\
  );\
  CREATE TABLE IF NOT EXISTS schedule_cache (\
    hex_id TEXT,\
    hour INTEGER,\
    priority REAL,\
    ts TEXT\
  );\
"

# Default CMD: start MCP stdio bridge and run Lua FOG router once as a sample evaluation.
CMD ["/bin/sh", "-c", "\
  echo 'hex_stability_carbon_snapshot' | ./mcp_stdio_governance_bridge; \
  lua5.3 lua/fog/fog_router.lua \
"]
