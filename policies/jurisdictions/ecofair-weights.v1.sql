CREATE TABLE IF NOT EXISTS ecofair_weights (
    jurisdiction_id     TEXT NOT NULL,
    sko_class           TEXT NOT NULL,
    w_energy            REAL NOT NULL,
    w_carbon            REAL NOT NULL,
    w_latency           REAL NOT NULL,
    w_risk              REAL NOT NULL,
    w_fair              REAL NOT NULL,
    constitutional_basis TEXT NOT NULL,
    validity_start_date TEXT NOT NULL,
    validity_end_date   TEXT,
    PRIMARY KEY (jurisdiction_id, sko_class),
    FOREIGN KEY (jurisdiction_id) REFERENCES jurisdictions(jurisdiction_id),
    FOREIGN KEY (sko_class)       REFERENCES sko_classes(sko_class),
    CHECK (w_energy  >= 0.0 AND w_carbon >= 0.0 AND
           w_latency >= 0.0 AND w_risk   >= 0.0 AND
           w_fair    >= 0.0),
    CHECK (ABS(w_energy + w_carbon + w_latency + w_risk + w_fair - 1.0) < 1e-6)
);
