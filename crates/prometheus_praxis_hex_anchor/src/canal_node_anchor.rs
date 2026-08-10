// File: crates/prometheus_praxis_hex_anchor/src/canal_node_anchor.rs
#![forbid(unsafe_code)]

use std::collections::HashMap;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct HexGridCoordinate {
    pub q: i32,
    pub r: i32,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CanalNodeAnchor {
    pub canal_node: String,
    pub coordinate: HexGridCoordinate,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CanalAnchorError {
    EmptyCanalNode,
    DuplicateCanalNode,
    UnknownCanalNode,
    CoordinateOccupied,
}

#[derive(Default)]
pub struct CanalNodeAnchorIndex {
    by_node: HashMap<String, HexGridCoordinate>,
    by_coordinate: HashMap<HexGridCoordinate, String>,
}

impl CanalNodeAnchorIndex {
    pub fn register(&mut self, anchor: CanalNodeAnchor) -> Result<(), CanalAnchorError> {
        if anchor.canal_node.trim().is_empty() {
            return Err(CanalAnchorError::EmptyCanalNode);
        }
        if self.by_node.contains_key(&anchor.canal_node) {
            return Err(CanalAnchorError::DuplicateCanalNode);
        }
        if self.by_coordinate.contains_key(&anchor.coordinate) {
            return Err(CanalAnchorError::CoordinateOccupied);
        }

        self.by_coordinate
            .insert(anchor.coordinate, anchor.canal_node.clone());
        self.by_node.insert(anchor.canal_node, anchor.coordinate);
        Ok(())
    }

    pub fn coordinate_for(
        &self,
        canal_node: &str,
    ) -> Result<HexGridCoordinate, CanalAnchorError> {
        self.by_node
            .get(canal_node)
            .copied()
            .ok_or(CanalAnchorError::UnknownCanalNode)
    }

    pub fn canal_node_at(
        &self,
        coordinate: HexGridCoordinate,
    ) -> Result<&str, CanalAnchorError> {
        self.by_coordinate
            .get(&coordinate)
            .map(String::as_str)
            .ok_or(CanalAnchorError::UnknownCanalNode)
    }

    pub fn within_radius(
        &self,
        origin: HexGridCoordinate,
        radius: u32,
    ) -> Vec<CanalNodeAnchor> {
        self.by_node
            .iter()
            .filter_map(|(canal_node, coordinate)| {
                (hex_distance(origin, *coordinate) <= radius).then(|| CanalNodeAnchor {
                    canal_node: canal_node.clone(),
                    coordinate: *coordinate,
                })
            })
            .collect()
    }
}

pub fn hex_distance(left: HexGridCoordinate, right: HexGridCoordinate) -> u32 {
    let dx = i64::from(left.q) - i64::from(right.q);
    let dz = i64::from(left.r) - i64::from(right.r);
    let dy = -dx - dz;
    ((dx.abs() + dy.abs() + dz.abs()) / 2) as u32
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_canal_node_and_supports_spatial_queries() {
        let mut index = CanalNodeAnchorIndex::default();
        index.register(CanalNodeAnchor {
            canal_node: "phx-canal-a".into(),
            coordinate: HexGridCoordinate { q: 4, r: -2 },
        }).unwrap();
        index.register(CanalNodeAnchor {
            canal_node: "phx-canal-b".into(),
            coordinate: HexGridCoordinate { q: 5, r: -2 },
        }).unwrap();

        assert_eq!(
            index.coordinate_for("phx-canal-a").unwrap(),
            HexGridCoordinate { q: 4, r: -2 }
        );
        assert_eq!(index.within_radius(HexGridCoordinate { q: 4, r: -2 }, 1).len(), 2);
    }
}
