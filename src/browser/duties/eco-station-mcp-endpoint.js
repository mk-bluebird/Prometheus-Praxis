// src/browser/duties/eco-station-mcp-endpoint.js

import http from "node:http";
import { createEcoStationRegistry } from "./eco-station-registry.js";

/**
 * Start an HTTP JSON endpoint for ECO Station queries.
 * Agents send { action, params } and receive structured results.
 */
export function startEcoStationApiServer({ port, geojson }) {
  const registry = createEcoStationRegistry(geojson);

  const server = http.createServer((req, res) => {
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
        const { action, params = {} } = payload;

        let result;

        switch (action) {
          case "listAll":
            result = registry.listAll();
            break;
          case "getByObjectId":
            result = registry.getByObjectId(params.objectId);
            break;
          case "searchByText":
            result = registry.searchByText(params.query || "");
            break;
          case "queryByRadius":
            result = registry.queryByRadius(
              params.lat,
              params.lon,
              params.radiusKm || 5
            );
            break;
          default:
            res.writeHead(400, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ error: `Unknown action: ${action}` }));
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

  server.listen(port);
  return server;
}
