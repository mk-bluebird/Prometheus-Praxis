"use strict";

import { generatePhoenixPilotsMarkdownTable } from "../src/browser/util/markdown-pilots-table.js";

function main() {
  const table = generatePhoenixPilotsMarkdownTable();
  // In a real repo you would write to a file; here we just log.
  process.stdout.write(table + "\n");
}

main();
