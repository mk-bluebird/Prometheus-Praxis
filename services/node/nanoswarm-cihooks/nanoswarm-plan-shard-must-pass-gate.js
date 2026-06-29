// Filename: services/node/nanoswarm-cihooks/nanoswarm-plan-shard-must-pass-gate.js
// Domain: Prometheus-Praxis CI
// Purpose: Enforce "no shard row, no mission" for nanoswarm plan KOs.

'use strict';

const { spawnSync } = require('child_process');
const path = require('path');

function runLuaShardGate(planKoId, dbPath, shardTable) {
  const luaScript = path.join(
    __dirname,
    '..',
    '..',
    '..',
    'runtime',
    'lua',
    'prometheus_praxis',
    'ci',
    'ppx_nanoswarm_plan_shard_gate.lua'
  );

  const input = JSON.stringify({
    plan_ko_id: planKoId,
    db_path: dbPath,
    shard_table_name: shardTable
  });

  const proc = spawnSync('lua', [luaScript], {
    input,
    encoding: 'utf8'
  });

  if (proc.error) {
    throw new Error('Lua shard gate exec failed: ' + proc.error.message);
  }

  if (proc.status !== 0) {
    throw new Error('Lua shard gate exited non-zero: ' + proc.stdout + proc.stderr);
  }

  let result;
  try {
    result = JSON.parse(proc.stdout.trim());
  } catch (e) {
    throw new Error('Lua shard gate returned non-JSON: ' + proc.stdout);
  }

  return result;
}

/**
 * CI entrypoint.
 * moduleSpec: JSON specifying nanoswarm plan module.
 * Expected fields:
 *   - moduleSpec.nanoswarmPlanKoId
 *   - moduleSpec.ecosafetyDbPath
 *   - moduleSpec.nanoswarmShardTable (optional, default 'nanoswarm_urban_shards')
 */
function nanoswarmPlanShardMustPassGate(moduleSpec) {
  const planKoId = moduleSpec.nanoswarmPlanKoId;
  const dbPath = moduleSpec.ecosafetyDbPath;
  const shardTable = moduleSpec.nanoswarmShardTable || 'nanoswarm_urban_shards';

  if (!planKoId) {
    throw new Error('PPXCI_NANOSWARM_PLAN_KO_ID_MISSING');
  }
  if (!dbPath) {
    throw new Error('PPXCI_ECOSAFETY_DB_PATH_MISSING');
  }

  const result = runLuaShardGate(planKoId, dbPath, shardTable);

  if (!result.ok) {
    const msg = (result.violations || []).join('; ');
    throw new Error(
      'PPXCI_NANOSWARM_PLAN_SHARD_GATE_FAILED for planKoId=' +
        planKoId +
        ' violations=' +
        msg
    );
  }

  return true;
}

if (require.main === module) {
  try {
    const specJson = process.argv[2];
    if (!specJson) {
      throw new Error('Usage: nanoswarm-plan-shard-must-pass-gate \'{"...": "..."}\'');
    }
    const moduleSpec = JSON.parse(specJson);
    nanoswarmPlanShardMustPassGate(moduleSpec);
    console.log('nanoswarm-plan-shard-must-pass-gate OK for module', moduleSpec.name || '<unnamed>');
    process.exit(0);
  } catch (err) {
    console.error('nanoswarm-plan-shard-must-pass-gate FAILED -', err.message);
    process.exit(1);
  }
}

module.exports = {
  nanoswarmPlanShardMustPassGate
};
