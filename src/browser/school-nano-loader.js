// src/browser/school-nano-loader.js

import { SchoolNanoCorridorShard } from '../aln/school-nano-corridor-parser.js';
import { SchoolNanoGovernor } from '../query/school-nano-governor.js';

/**
 * Load the school nano corridor ALN shard via fetch and return a governor.
 * @param {string} url - HTTP(s) URL or relative path to TexasArizonaSchoolNanoCorridorGovernance2026v2.aln
 * @returns {Promise<SchoolNanoGovernor>}
 */
export async function loadSchoolNanoGovernor(url) {
  const res = await fetch(url, {
    method: 'GET',
    headers: {
      'Accept': 'text/plain'
    }
  });

  if (!res.ok) {
    throw new Error(`Failed to load ALN shard from ${url}: ${res.status} ${res.statusText}`);
  }

  const text = await res.text();
  const shard = SchoolNanoCorridorShard.fromString(text);
  return new SchoolNanoGovernor(shard);
}

/**
 * Example: query nano pilot candidates in Texas.
 * This is a utility function you can call from UI or agent code.
 * @param {string} url
 * @returns {Promise<object[]>}
 */
export async function findTexasNanoPilotCandidates(url) {
  const gov = await loadSchoolNanoGovernor(url);
  const txRows = gov.byState('TX');
  // Reuse the governance thresholds tuned for cautious pilots
  const candidates = gov.nanoPilotCandidates({
    rRegulatoryMax: 0.10,
    rConsentMax: 0.30,
    rLongtermMax: 0.35
  });

  // Intersect with Texas rows
  const txSet = new Set(txRows.map(r => r.region_id));
  return candidates.filter(r => txSet.has(r.region_id));
}
