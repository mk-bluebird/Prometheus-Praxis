# filename setup_db.sh
# destination eco_restoration_shard/scripts/setup_db.sh
#!/usr/bin/env sh
set -eu

# 37. setup_db.sh: build eco_constellation.db from db/*.sql, with integrity check.

TARGET_DIR="${1:-.}"
DB_PATH="${TARGET_DIR%/}/eco_constellation.db"

mkdir -p "$TARGET_DIR"

if [ -f "$DB_PATH" ]; then
  rm -f "$DB_PATH"
fi

SQLITE_BIN="${SQLITE_BIN:-sqlite3}"

enable_fk_and_integrity_check() {
  "$SQLITE_BIN" "$DB_PATH" <<'EOF'
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA integrity_check;
EOF
}

# Load core schema first, then extensions, then views.
# Ordering is explicit to keep idempotence and avoid missing-table errors.
for sql_file in \
  "db/db_ecoconstellationindex.sql" \
  "db/db_eco_planes.sql" \
  "db/db_shardinstance.sql" \
  "db/db_eco_author_evidence.sql" \
  "db/db_eco_repo_identity_binding.sql" \
  "db/db_knowledgeecoscore.sql" \
  "db/db_lanestatusshard.sql" \
  "db/db_eco_author_evidence_source_kind.sql" \
  "db/db_eco_github_migration_attribution.sql" \
  "db/db_ker_retro_score_doctor0evil.sql" \
  "db/db_v_eco_restoration_shard_research.sql"
do
  if [ -f "$sql_file" ]; then
    echo "Loading $sql_file into $DB_PATH"
    "$SQLITE_BIN" "$DB_PATH" < "$sql_file"
  fi
done

enable_fk_and_integrity_check

echo "eco_constellation.db initialized at $DB_PATH"
