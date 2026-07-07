//! large_particle_registry library

pub fn hello() -> &'static str {
    "large_particle_registry"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "large_particle_registry");
    }
}
