-- File: sql/eco_restoration/hex_anchor_hierarchy.sql

CREATE TABLE IF NOT EXISTS hex_anchor (
    anchor INTEGER PRIMARY KEY,
    level INTEGER NOT NULL CHECK(level BETWEEN 0 AND 7),
    row_index INTEGER NOT NULL CHECK(row_index BETWEEN 0 AND 1073741823),
    column_index INTEGER NOT NULL CHECK(column_index BETWEEN 0 AND 1073741823),
    CHECK(anchor = ((level << 60) | (row_index << 30) | column_index)),
    CHECK((row_index & ((1 << (7 - level)) - 1)) = 0),
    CHECK((column_index & ((1 << (7 - level)) - 1)) = 0)
) STRICT;

CREATE INDEX IF NOT EXISTS hex_anchor_level_row_column
ON hex_anchor(level, row_index, column_index);

WITH RECURSIVE
input(anchor, target_level) AS (
    VALUES(:anchor, :target_level)
),
root(level, row_index, column_index, target_level) AS (
    SELECT
        (anchor >> 60) & 15,
        (anchor >> 30) & 1073741823,
        anchor & 1073741823,
        target_level
    FROM input
    WHERE target_level BETWEEN ((anchor >> 60) & 15) AND 7
),
descendants(level, row_index, column_index, target_level) AS (
    SELECT level, row_index, column_index, target_level FROM root
    UNION ALL
    SELECT
        descendants.level + 1,
        (descendants.row_index << 1) | branch.row_bit,
        (descendants.column_index << 1) | branch.column_bit,
        descendants.target_level
    FROM descendants
    CROSS JOIN (
        SELECT 0 AS row_bit, 0 AS column_bit
        UNION ALL SELECT 0, 1
        UNION ALL SELECT 1, 0
        UNION ALL SELECT 1, 1
    ) AS branch
    WHERE descendants.level < descendants.target_level
)
SELECT
    ((level << 60) | (row_index << 30) | column_index) AS descendant_anchor,
    level,
    row_index,
    column_index
FROM descendants
ORDER BY level, row_index, column_index;

WITH RECURSIVE
input(anchor) AS (
    VALUES(:anchor)
),
lineage(anchor, level, row_index, column_index) AS (
    SELECT
        anchor,
        (anchor >> 60) & 15,
        (anchor >> 30) & 1073741823,
        anchor & 1073741823
    FROM input
    UNION ALL
    SELECT
        ((level - 1) << 60) | ((row_index >> 1) << 30) | (column_index >> 1),
        level - 1,
        row_index >> 1,
        column_index >> 1
    FROM lineage
    WHERE level > 0
)
SELECT anchor, level, row_index, column_index
FROM lineage
ORDER BY level;
