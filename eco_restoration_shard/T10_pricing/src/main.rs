// filename: T10_pricing/src/main.rs

use t10_pricing::{compute_eco_price, EcoPricingInput};

fn main() {
    let input = EcoPricingInput {
        base_cost: 1.0,
        r_carbon: 0.2,
        r_energy: 0.1,
        r_biodiversity: 0.3,
    };

    let output = compute_eco_price(&input).expect("pricing computation failed");
    println!("Eco-aware pricing: {:?}", output);
}
