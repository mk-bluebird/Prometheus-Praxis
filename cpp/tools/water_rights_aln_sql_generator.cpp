// File: cpp/tools/water_rights_aln_sql_generator.cpp
#include <iostream>

namespace eco {

const char* ALN_WATER_RIGHTS = R"ALN(
entity WaterRights {
  fields {
    right_id        : Text;
    holder_id       : Text;   // person, entity, or hex aggregate
    source_id       : Text;   // canal reach, well, surface water
    daily_limit_m3  : Float >= 0.0;
    seasonal_limit_m3 : Float >= 0.0;
    priority_class  : Integer; // 1 = highest priority
    is_exempt       : Bool;
    allocated_today_m3   : Float >= 0.0;
    allocated_season_m3  : Float >= 0.0;
  }

  invariant daily_limit_non_negative {
    daily_limit_m3 >= 0.0;
    allocated_today_m3 >= 0.0;
    allocated_today_m3 <= daily_limit_m3 || is_exempt;
  }

  invariant seasonal_limit_non_negative {
    seasonal_limit_m3 >= 0.0;
    allocated_season_m3 >= 0.0;
    allocated_season_m3 <= seasonal_limit_m3 || is_exempt;
  }

  invariant priority_ordering {
    // Lower priority_class integer means higher legal priority.
    priority_class >= 1;
  }
}
)ALN";

const char* SQL_WATER_RIGHTS_SCHEMA = R"SQL(
-- SQL schema corresponding to WaterRights ALN v2 entity.

CREATE TABLE IF NOT EXISTS water_rights (
  right_id              TEXT PRIMARY KEY,
  holder_id             TEXT NOT NULL,
  source_id             TEXT NOT NULL,
  daily_limit_m3        REAL NOT NULL DEFAULT 0.0,
  seasonal_limit_m3     REAL NOT NULL DEFAULT 0.0,
  priority_class        INTEGER NOT NULL,
  is_exempt             INTEGER NOT NULL DEFAULT 0, -- 0=false, 1=true
  allocated_today_m3    REAL NOT NULL DEFAULT 0.0,
  allocated_season_m3   REAL NOT NULL DEFAULT 0.0
);

-- Log table for individual allocations
CREATE TABLE IF NOT EXISTS water_rights_log (
  log_id                INTEGER PRIMARY KEY AUTOINCREMENT,
  right_id              TEXT NOT NULL,
  quantity_m3           REAL NOT NULL,
  ts                    TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
)SQL";

const char* SQL_WATER_RIGHTS_TRIGGERS = R"SQL(
-- BEFORE INSERT/UPDATE trigger enforcing WaterRights invariants.

CREATE TRIGGER IF NOT EXISTS tr_water_rights_before_ins
BEFORE INSERT ON water_rights
FOR EACH ROW
BEGIN
  -- daily_limit_m3 and seasonal_limit_m3 must be non-negative
  CASE
    WHEN NEW.daily_limit_m3 < 0.0 THEN
      RAISE(ABORT, 'WaterRights: daily_limit_m3 must be >= 0');
    WHEN NEW.seasonal_limit_m3 < 0.0 THEN
      RAISE(ABORT, 'WaterRights: seasonal_limit_m3 must be >= 0');
    WHEN NEW.priority_class < 1 THEN
      RAISE(ABORT, 'WaterRights: priority_class must be >= 1');
  END;
END;

CREATE TRIGGER IF NOT EXISTS tr_water_rights_before_upd
BEFORE UPDATE ON water_rights
FOR EACH ROW
BEGIN
  -- Non-negative limits and allocations
  CASE
    WHEN NEW.daily_limit_m3 < 0.0 THEN
      RAISE(ABORT, 'WaterRights: daily_limit_m3 must be >= 0');
    WHEN NEW.seasonal_limit_m3 < 0.0 THEN
      RAISE(ABORT, 'WaterRights: seasonal_limit_m3 must be >= 0');
    WHEN NEW.allocated_today_m3 < 0.0 THEN
      RAISE(ABORT, 'WaterRights: allocated_today_m3 must be >= 0');
    WHEN NEW.allocated_season_m3 < 0.0 THEN
      RAISE(ABORT, 'WaterRights: allocated_season_m3 must be >= 0');
    WHEN NEW.priority_class < 1 THEN
      RAISE(ABORT, 'WaterRights: priority_class must be >= 1');
  END;

  -- Daily and seasonal bounds unless exempt
  CASE
    WHEN NEW.is_exempt = 0 AND NEW.allocated_today_m3 > NEW.daily_limit_m3 THEN
      RAISE(ABORT, 'WaterRights: daily allocation exceeds daily_limit_m3');
    WHEN NEW.is_exempt = 0 AND NEW.allocated_season_m3 > NEW.seasonal_limit_m3 THEN
      RAISE(ABORT, 'WaterRights: seasonal allocation exceeds seasonal_limit_m3');
  END;
END;

-- BEFORE INSERT on water_rights_log: update allocations and enforce limits via above trigger.
CREATE TRIGGER IF NOT EXISTS tr_water_rights_log_before_ins
BEFORE INSERT ON water_rights_log
FOR EACH ROW
BEGIN
  -- Update allocated_today_m3 and allocated_season_m3 for the corresponding right_id.
  UPDATE water_rights
  SET allocated_today_m3 = allocated_today_m3 + NEW.quantity_m3,
      allocated_season_m3 = allocated_season_m3 + NEW.quantity_m3
  WHERE right_id = NEW.right_id;

  -- The update will be validated by tr_water_rights_before_upd; any violation aborts.
END;
)SQL";

void print_water_rights_specs() {
    std::cout << "=== ALN v2: WaterRights ===\n\n";
    std::cout << ALN_WATER_RIGHTS << "\n\n";

    std::cout << "=== SQL Schema: WaterRights ===\n\n";
    std::cout << SQL_WATER_RIGHTS_SCHEMA << "\n\n";

    std::cout << "=== SQL Triggers: WaterRights Invariants ===\n\n";
    std::cout << SQL_WATER_RIGHTS_TRIGGERS << "\n";
}

} // namespace eco

int main() {
    eco::print_water_rights_specs();
    return 0;
}
