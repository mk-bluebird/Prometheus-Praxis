//! education_prompts library

pub fn hello() -> &'static str {
    "education_prompts"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "education_prompts");
    }
}
