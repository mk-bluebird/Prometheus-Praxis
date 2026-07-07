//! bostrom_cli library

pub fn hello() -> &'static str {
    "bostrom_cli"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "bostrom_cli");
    }
}
