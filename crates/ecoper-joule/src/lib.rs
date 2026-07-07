//! ecoper_joule library

pub fn hello() -> &'static str {
    "ecoper_joule"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "ecoper_joule");
    }
}
