// src/habitat/habitat-mcp-endpoint.js

import http from "node:http";
import { createHabitatServiceFromJson } from "./habitat-service.js";

/**
 * Minimal HTTP endpoint exposing habitat queries for AI-chat agents.
 * In production, you would plug this into your browser-duty system /
 * MCP server rather than running raw HTTP.[file:79]
 */

export function startHabitatApiServer({ port, habitatJson }) {
  const service = createHabitatServiceFromJson(habitatJson);

  const server = http.createServer(async (req, res) => {
    if (req.method !== "POST") {
      res.writeHead(405, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "POST only" }));
      return;
    }

    let body = "";
    req.on("data", (chunk) => {
      body += chunk;
    });

    req.on("end", () => {
      try {
        const payload = JSON.parse(body || "{}");
        const { action, params } = payload;

        let result;
        switch (action) {
          case "listHabitats":
            result = service.listHabitats(params || {});
            break;
          case "getHabitatDetail":
            result = service.getHabitatDetail(params?.habitatId);
            break;
          case "queryHabitatsBySpecies":
            result = service.queryHabitatsBySpecies(params || {});
            break;
          case "queryHabitatsForDredge":
            result = service.queryHabitatsForDredge(params || {});
            break;
          default:
            res.writeHead(400, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ error: "Unknown action" }));
            return;
        }

        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, result }));
      } catch (err) {
        res.writeHead(500, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ error: err.message }));
      }
    });
  });

  server.listen(port, () => {
    // No console logging needed for agents; host can observe via telemetry.
  });

  return server;
}
