//! eco_restoration_shard_core library
//! 
//! Rust governance spine client for the eco_restoration_shard Phoenix governance DB

pub fn hello() -> &'static str {
    "eco_restoration_shard_core"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "eco_restoration_shard_core");
    }
}
