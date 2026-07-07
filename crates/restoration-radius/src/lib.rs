//! restoration_radius library

pub fn hello() -> &'static str {
    "restoration_radius"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "restoration_radius");
    }
}
