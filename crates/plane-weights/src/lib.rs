//! plane_weights library

pub fn hello() -> &'static str {
    "plane_weights"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "plane_weights");
    }
}
