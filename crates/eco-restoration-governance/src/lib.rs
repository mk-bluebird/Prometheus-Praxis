//! eco_restoration_governance library

pub fn hello() -> &'static str {
    "eco_restoration_governance"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "eco_restoration_governance");
    }
}
