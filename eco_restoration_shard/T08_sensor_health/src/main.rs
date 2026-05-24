// filename: T08_sensor_health/src/main.rs

use chrono::Utc;
use t08_sensor_health::{analyze_sensor_health, SensorReading};

fn main() {
    let sensor_id = "example_sensor";
    let readings = vec![
        SensorReading {
            timestamp: Utc::now(),
            value: 1.0,
        },
        SensorReading {
            timestamp: Utc::now(),
            value: 1.2,
        },
        SensorReading {
            timestamp: Utc::now(),
            value: 0.9,
        },
    ];

    let summary = analyze_sensor_health(sensor_id, &readings, 5)
        .expect("sensor health analysis failed");
    println!("Sensor health summary: {:?}", summary);
}
