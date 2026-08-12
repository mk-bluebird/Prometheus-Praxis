-- File: sql/eco_restoration/hex_action_dependency_map.sql

CREATE TABLE IF NOT EXISTS hex_action_node (
    action_id INTEGER PRIMARY KEY,
    hex_anchor INTEGER NOT NULL,
    action TEXT NOT NULL,
    priority REAL NOT NULL CHECK(priority BETWEEN 0 AND 1),
    state TEXT NOT NULL CHECK(state IN ('PENDING','COMPLETED','BLOCKED')),
    UNIQUE(hex_anchor, action),
    CHECK(action IN (
        'tree_planting','canal_cleaning','native_seedling','mulch_application',
        'infiltration_basin','habitat_monitoring','soil_amendment'
    ))
) STRICT;

CREATE TABLE IF NOT EXISTS hex_action_dependency (
    action_id INTEGER NOT NULL REFERENCES hex_action_node(action_id),
    prerequisite_action_id INTEGER NOT NULL REFERENCES hex_action_node(action_id),
    PRIMARY KEY(action_id, prerequisite_action_id),
    CHECK(action_id <> prerequisite_action_id)
) STRICT;

CREATE INDEX IF NOT EXISTS hex_action_priority_index
ON hex_action_node(hex_anchor, state, priority DESC);

CREATE TRIGGER IF NOT EXISTS hex_action_dependency_anchor_match
BEFORE INSERT ON hex_action_dependency
FOR EACH ROW
WHEN (
    SELECT hex_anchor FROM hex_action_node WHERE action_id = NEW.action_id
) <> (
    SELECT hex_anchor FROM hex_action_node WHERE action_id = NEW.prerequisite_action_id
)
BEGIN
    SELECT RAISE(ABORT, 'action dependencies must remain within one hex anchor');
END;

WITH RECURSIVE
dependency_tree(action_id, prerequisite_action_id, depth) AS (
    SELECT action_id, prerequisite_action_id, 1
    FROM hex_action_dependency
    UNION ALL
    SELECT tree.action_id, dependency.prerequisite_action_id, tree.depth + 1
    FROM dependency_tree AS tree
    JOIN hex_action_dependency AS dependency
      ON dependency.action_id = tree.prerequisite_action_id
),
dependency_depth(action_id, maximum_depth) AS (
    SELECT action_id, MAX(depth)
    FROM dependency_tree
    GROUP BY action_id
)
SELECT
    node.action_id,
    node.hex_anchor,
    node.action,
    node.priority,
    COALESCE(depth.maximum_depth, 0) AS dependency_depth
FROM hex_action_node AS node
LEFT JOIN dependency_depth AS depth ON depth.action_id = node.action_id
WHERE node.hex_anchor = :hex_anchor
  AND node.state = 'PENDING'
  AND NOT EXISTS (
      SELECT 1
      FROM dependency_tree AS tree
      JOIN hex_action_node AS prerequisite ON prerequisite.action_id = tree.prerequisite_action_id
      WHERE tree.action_id = node.action_id
        AND prerequisite.state <> 'COMPLETED'
  )
ORDER BY dependency_depth ASC, node.priority DESC, node.action_id ASC;
