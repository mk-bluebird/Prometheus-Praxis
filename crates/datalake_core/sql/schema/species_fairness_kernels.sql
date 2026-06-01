PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS species_fairness_kernels (
    sko_class       TEXT NOT NULL,
    jurisdiction_id TEXT NOT NULL,
    species_id      TEXT NOT NULL,
    alpha_kp        REAL NOT NULL CHECK (alpha_kp >= 0.0 AND alpha_kp <= 1.0),
    provenance      TEXT NOT NULL,
    status          TEXT NOT NULL CHECK (status IN ('draft','validated')),
    PRIMARY KEY (sko_class, jurisdiction_id, species_id),
    FOREIGN KEY (sko_class)       REFERENCES sko_classes(sko_class),
    FOREIGN KEY (jurisdiction_id) REFERENCES jurisdictions(jurisdiction_id),
    FOREIGN KEY (species_id)      REFERENCES species_registry(species_id)
);
