//! lane_governance library

pub fn hello() -> &'static str {
    "lane_governance"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "lane_governance");
    }
}
