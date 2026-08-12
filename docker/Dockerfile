# File: Dockerfile
FROM ubuntu:24.04 AS build

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libsqlite3-dev \
    libgdal-dev \
    libeigen3-dev \
    libsndfile1-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY cpp ./cpp
COPY lua ./lua
COPY CMakeLists.txt ./CMakeLists.txt

RUN test -d cpp/tools \
    && test -f cpp/tools/mcp_stdio_governance_bridge.cpp \
    && test -f cpp/tools/fog_schedule_predictor.cpp \
    && g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
        cpp/tools/mcp_stdio_governance_bridge.cpp \
        -o /out/mcp_stdio_governance_bridge \
        $(pkg-config --cflags --libs sqlite3) \
    && g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
        cpp/tools/fog_schedule_predictor.cpp \
        -o /out/fog_schedule_predictor \
        $(pkg-config --cflags --libs sqlite3)

FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    sqlite3 \
    lua5.4 \
    lua-sql-sqlite3 \
    libsqlite3-0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --uid 10001 eco

WORKDIR /app
COPY --from=build /out/mcp_stdio_governance_bridge /usr/local/bin/mcp_stdio_governance_bridge
COPY --from=build /out/fog_schedule_predictor /usr/local/bin/fog_schedule_predictor
COPY lua ./lua
COPY docker/entrypoint.sh /usr/local/bin/prometheus-praxis-entrypoint

RUN chmod 0755 /usr/local/bin/mcp_stdio_governance_bridge \
    /usr/local/bin/fog_schedule_predictor \
    /usr/local/bin/prometheus-praxis-entrypoint \
    && mkdir -p /data \
    && chown eco:eco /data

USER eco
VOLUME ["/data"]
ENTRYPOINT ["/usr/local/bin/prometheus-praxis-entrypoint"]
CMD ["/bin/sh", "-c", "printf '%s\n' 'hex_stability_carbon_snapshot' | mcp_stdio_governance_bridge && lua5.4 lua/fog/fog_router.lua"]
