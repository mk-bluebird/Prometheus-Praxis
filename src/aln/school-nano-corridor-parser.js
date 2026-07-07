// src/aln/school-nano-corridor-parser.js

/**
 * Minimal ALN parser for TexasArizonaSchoolNanoCorridorGovernance2026v2.aln
 * Assumptions:
 * - Single META block, one RECORD definition, one SECTION rows block.
 * - SECTION rows SchoolNanoCorridorRow ... contains CSV-like rows separated by newlines.
 * - Fields are defined in the RECORD in the correct order.
 * - Quoted fields may contain commas; unquoted fields do not.
 */

export class SchoolNanoCorridorShard {
  constructor(meta, schema, rows) {
    this.meta = meta;       // { key: value }
    this.schema = schema;   // [ { name, type } ]
    this.rows = rows;       // [ { fieldName: value, ... } ]
  }

  /**
   * Load and parse ALN shard from a raw string.
   * @param {string} alnText
   * @returns {SchoolNanoCorridorShard}
   */
  static fromString(alnText) {
    const lines = alnText
      .split(/\r?\n/)
      .map(l => l.trim())
      .filter(l => l.length > 0 && !l.startsWith('#'));

    const meta = {};
    const schema = [];
    const rawSectionLines = [];

    let inMeta = false;
    let inRecord = false;
    let inSection = false;

    for (const line of lines) {
      if (line.startsWith('META ')) {
        inMeta = true;
        inRecord = false;
        inSection = false;
        continue;
      }
      if (line === 'ENDMETA') {
        inMeta = false;
        continue;
      }
      if (line.startsWith('RECORD ')) {
        inMeta = false;
        inRecord = true;
        inSection = false;
        continue;
      }
      if (line.startsWith('ENDRECORD')) {
        inRecord = false;
        continue;
      }
      if (line.startsWith('SECTION ')) {
        inMeta = false;
        inRecord = false;
        inSection = true;
        continue;
      }
      if (line === 'ENDSECTION') {
        inSection = false;
        continue;
      }

      if (inMeta) {
        const [key, ...rest] = line.split(/\s+/);
        meta[key] = rest.join(' ');
      } else if (inRecord) {
        // Expect lines like: field_name  type  # comment
        const commentSplit = line.split('#');
        const left = commentSplit[0].trim();
        if (!left) continue;
        const parts = left.split(/\s+/);
        const name = parts[0];
        const type = parts[1] || 'text';
        schema.push({ name, type });
      } else if (inSection) {
        rawSectionLines.push(line);
      }
    }

    const rows = parseSectionRows(schema, rawSectionLines.join('\n'));

    return new SchoolNanoCorridorShard(meta, schema, rows);
  }
}

/**
 * Parse the SECTION rows body into structured objects using schema order.
 * Supports simple CSV with quoted fields.
 * @param {Array<{name: string, type: string}>} schema
 * @param {string} sectionBody
 * @returns {Array<object>}
 */
function parseSectionRows(schema, sectionBody) {
  // Normalize: split on newlines, remove trailing commas-only lines.
  const allLines = sectionBody
    .split(/\r?\n/)
    .map(l => l.trim())
    .filter(l => l.length > 0);

  // The SECTION in your ALN example is a single logical CSV stream.
  // We rebuild a single CSV text and then chunk it back into records.
  const csvText = allLines.join('\n');

  const records = [];
  let currentFields = [];
  let currentBuffer = '';
  let inQuotes = false;

  // Simple state machine CSV parser that is newline-aware.
  for (let i = 0; i < csvText.length; i++) {
    const ch = csvText[i];

    if (ch === '"') {
      if (inQuotes && csvText[i + 1] === '"') {
        // Escaped quote
        currentBuffer += '"';
        i++;
      } else {
        inQuotes = !inQuotes;
      }
      continue;
    }

    if (!inQuotes && ch === ',') {
      currentFields.push(currentBuffer);
      currentBuffer = '';
      continue;
    }

    if (!inQuotes && (ch === '\n' || ch === '\r')) {
      // End of record
      if (currentBuffer.length > 0 || currentFields.length > 0) {
        currentFields.push(currentBuffer);
        const record = buildRowFromFields(schema, currentFields);
        records.push(record);
        currentFields = [];
        currentBuffer = '';
      }
      continue;
    }

    currentBuffer += ch;
  }

  // Flush last record if present
  if (currentBuffer.length > 0 || currentFields.length > 0) {
    currentFields.push(currentBuffer);
    const record = buildRowFromFields(schema, currentFields);
    records.push(record);
  }

  return records;
}

/**
 * Convert a flat array of field strings into a typed row object.
 * @param {Array<{name: string, type: string}>} schema
 * @param {string[]} fields
 * @returns {object}
 */
function buildRowFromFields(schema, fields) {
  const row = {};
  const n = Math.min(schema.length, fields.length);

  for (let i = 0; i < n; i++) {
    const { name, type } = schema[i];
    const raw = fields[i].trim();
    row[name] = coerceType(raw, type);
  }

  // If extra fields exist, ignore them; if fewer, remaining stay undefined.
  return row;
}

/**
 * Coerce string into basic ALN types used in this shard.
 * @param {string} raw
 * @param {string} type
 * @returns {*}
 */
function coerceType(raw, type) {
  if (raw === '') return null;

  switch (type) {
    case 'int': {
      const v = parseInt(raw, 10);
      return Number.isNaN(v) ? null : v;
    }
    case 'float': {
      const v = parseFloat(raw);
      return Number.isNaN(v) ? null : v;
    }
    case 'bool': {
      const lower = raw.toLowerCase();
      if (lower === 'true') return true;
      if (lower === 'false') return false;
      return null;
    }
    case 'text':
    default: {
      // Strip surrounding quotes if present.
      const trimmed = raw.trim();
      if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.length >= 2) {
        return trimmed.slice(1, -1);
      }
      return trimmed;
    }
  }
}
