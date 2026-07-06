// FILE: src/edge/jetson-cyboquatic-server.js
"use strict";

const http = require("http");
const fs = require("fs");
const { createTileSpaceDutyFromPayload } = require("../browser/duties/tile-space-adapter.js");
const { MarineTxDuty } = require("../browser/duties/marine-tx-duty.js");
const { CyboquaticHardwareDuty } = require("../browser/duties/cyboquatic-hardware-duty.js");

// Load static config.
const eunrrEnvelope = JSON.parse(fs.readFileSync("config/eunrr-envelope.json", "utf8"));
const ecoValueWeights = JSON.parse(fs.readFileSync("config/eco-value-weights.json", "utf8"));

// In a real deployment, these would be refreshed from Rust/ALN APIs.
let tileSpacePayload = {/* fetched TileSpacePayload JSON */};
let marineTxArray = [/* MarineRestorationTx objects */];
let ecoValueSnapshots = [/* EcoValueSnapshot objects */];
let cyboquaticMachines = [/* CyboquaticMachine objects */];

const globalRoHCeiling = 0.30;

const tileDuty = createTileSpaceDutyFromPayload(tileSpacePayload);
const marineDuty = new MarineTxDuty(tileDuty, marineTxArray, ecoValueSnapshots, eunrrEnvelope);
const cyboDuty = new CyboquaticHardwareDuty(
  tileDuty,
  marineDuty,
  cyboquaticMachines,
  ecoValueWeights,
  globalRoHCeiling
);

const server = http.createServer((req, res) => {
  if (req.url === "/overlays/active" && req.method === "GET") {
    const tileSummary = tileDuty.summarizeActiveTile();
    const marineBadge = marineDuty.summarizeActiveMarineTile();
    const machineOverlays = cyboDuty.getActiveTileMachineOverlays();

    const payload = {
      tileSummary,
      marineBadge,
      machineOverlays,
    };

    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(payload));
    return;
  }

  if (req.url === "/overlays/neighbors" && req.method === "GET") {
    const neighborTiles = tileDuty.summarizeNeighborTiles();
    const neighborMarineBadges = marineDuty.summarizeNeighborMarineTiles();
    const neighborMachineOverlays = cyboDuty.getNeighborTileMachineOverlays();

    const payload = {
      neighborTiles,
      neighborMarineBadges,
      neighborMachineOverlays,
    };

    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(payload));
    return;
  }

  res.writeHead(404);
  res.end("Not found");
});

server.listen(8080, () => {
  console.log("Cyboquatic overlay server running on Jetson at http://localhost:8080");
});
