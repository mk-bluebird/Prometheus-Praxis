//! sensor_telemetry library

pub fn hello() -> &'static str {
    "sensor_telemetry"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "sensor_telemetry");
    }
}
