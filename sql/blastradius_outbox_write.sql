-- filename: sql/blastradius_outbox_write.sql
BEGIN IMMEDIATE;

INSERT INTO blastradiusdiag (
    diagid,
    nodeid,
    fogregionid,
    timestamputc,
    rhydraulic,
    rbio,
    rtox,
    vtbefore,
    vtafter,
    deltavt,
    kfactor,
    efactor,
    rfactor,
    kerscore,
    lane
) VALUES (
    :diagid,
    :nodeid,
    :fogregionid,
    :timestamputc,
    :rhydraulic,
    :rbio,
    :rtox,
    :vtbefore,
    :vtafter,
    :deltavt,
    :kfactor,
    :efactor,
    :rfactor,
    :kerscore,
    :lane
);

INSERT INTO blastradius_outbox (
    diagid,
    created_utc,
    status
) VALUES (
    :diagid,
    strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
    'PENDING'
);

COMMIT;
