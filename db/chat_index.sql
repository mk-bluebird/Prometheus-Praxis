-- filename: db/chat_index.sql
-- destination: ecorestorationshard/db/chat_index.sql
-- purpose:
--   PostgreSQL-style logical schema (for future Postgres mirror) describing
--   AI-chat facing indices over functions and files, aligned with econetfileindex.

CREATE TABLE IF NOT EXISTS chat_function_index (
    func_id        SERIAL PRIMARY KEY,
    language       TEXT NOT NULL,          -- 'lua','kotlin','rust'
    symbol_name    TEXT NOT NULL,          -- function or object name
    file_path      TEXT NOT NULL,          -- repo-relative path
    short_purpose  TEXT NOT NULL,          -- one-line description
    planes         TEXT,                   -- CSV of planes: 'energy,carbon,...'
    lane_default   TEXT,                   -- 'RESEARCH','EXPPROD','PROD'
    non_actuating  BOOLEAN NOT NULL,
    created_utc    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_chat_function_symbol
    ON chat_function_index(symbol_name);

CREATE INDEX IF NOT EXISTS idx_chat_function_lang_lane
    ON chat_function_index(language, lane_default);
