// filename: crates/ecosafety-core-v2/src/kalman_hyd_uhi.rs

#![forbid(unsafe_code)]

use nalgebra::{SMatrix, SVector, U5, U4};
use kfilter::Kalman; // or kalmanrs::LinearKalman, depending on chosen crate

pub struct HydUhiKalman {
    pub filter: Kalman<f64, 5, 4>,
}

impl HydUhiKalman {
    pub fn new() -> Self {
        // Define F, H, Q, R.
        let f: SMatrix<f64, U5, U5> = SMatrix::identity();
        // Set actual dynamics here (alpha, beta, gamma terms).
        let h: SMatrix<f64, U4, U5> = SMatrix::from_row_slice(&[
            1.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 1.0, 0.0,
        ]);
        let q: SMatrix<f64, U5, U5> = SMatrix::identity() * 0.01;
        let r: SMatrix<f64, U4, U4> = SMatrix::identity() * 0.05;

        // Initial state and covariance.
        let x0: SVector<f64, U5> = SVector::zeros();
        let p0: SMatrix<f64, U5, U5> = SMatrix::identity() * 0.1;

        let filter = Kalman::new(f, h, q, r, x0, p0);

        HydUhiKalman { filter }
    }

    pub fn predict(&mut self) {
        self.filter.predict();
    }

    pub fn update(&mut self, z: SVector<f64, U4>) {
        self.filter.update(z).expect("Kalman update failed");
    }

    pub fn state(&self) -> SVector<f64, U5> {
        self.filter.x
    }
}
