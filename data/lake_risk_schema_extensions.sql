-- filename: data/lake_risk_schema_extensions.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS agency_channel (
    channel_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    lake_or_basin        TEXT NOT NULL,
    agency_name          TEXT NOT NULL,
    channel_type         TEXT NOT NULL,
    url                  TEXT NOT NULL,
    jurisdiction_level   TEXT NOT NULL,
    notes                TEXT NOT NULL DEFAULT '',
    bostrom_credit_did   TEXT NOT NULL
);

INSERT INTO agency_channel (
    lake_or_basin,
    agency_name,
    channel_type,
    url,
    jurisdiction_level,
    notes,
    bostrom_credit_did
) VALUES
(
    'San Carlos Lake',
    'San Carlos Recreation and Wildlife Department',
    'facebook_page',
    'https://www.facebook.com/sancarlosrecreationandwildlife',
    'tribal',
    'Primary official social channel for closures and fish-kill notices at San Carlos Lake.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
),
(
    'Arizona statewide',
    'Arizona Game and Fish Department',
    'website',
    'https://www.azgfd.com',
    'state',
    'Central portal for statewide fisheries, fish-kill notices, and harmful algal bloom advisories.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);

CREATE TABLE IF NOT EXISTS social_signal (
    signal_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    lake_name             TEXT NOT NULL,
    platform              TEXT NOT NULL,
    url                   TEXT NOT NULL,
    event_type            TEXT NOT NULL,
    severity              TEXT NOT NULL,
    status                TEXT NOT NULL,
    received_at           TEXT NOT NULL,
    notes                 TEXT NOT NULL DEFAULT '',
    bostrom_credit_did    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS risk_score (
    score_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    lake_name             TEXT NOT NULL,
    timestamp             TEXT NOT NULL,
    risk_score_numeric    REAL NOT NULL,
    knowledge_factor      REAL NOT NULL,
    eco_impact_value      REAL NOT NULL,
    harm_risk_flag        INTEGER NOT NULL,
    notes                 TEXT NOT NULL DEFAULT '',
    bostrom_credit_did    TEXT NOT NULL
);
