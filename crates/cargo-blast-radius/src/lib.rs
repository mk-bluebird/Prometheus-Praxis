//! cargo_blast_radius library

pub fn hello() -> &'static str {
    "cargo_blast_radius"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "cargo_blast_radius");
    }
}
