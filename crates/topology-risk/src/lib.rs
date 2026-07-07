//! topology_risk library

pub fn hello() -> &'static str {
    "topology_risk"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "topology_risk");
    }
}
