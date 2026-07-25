// Filename: crates/ker_hex_interpolation/src/lib.rs
// Destination: Prometheus-Praxis/crates/ker_hex_interpolation/src/lib.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024
// rust-version: 1.85

#![forbid(unsafe_code)]

use std::f64;

/// Scalar KER score at a hex anchor: s = k * e - r, all in [0, 1].
/// This struct is designed to align with existing KER triad semantics
/// while providing a convenient carrier for kriging interpolation.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct KerScore {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

impl KerScore {
    /// Construct a KerScore, clamping each coordinate into [0, 1]
    /// and computing the canonical scalar s = k * e - r.
    pub fn new(k: f64, e: f64, r: f64) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        let k_c = clamp01(k);
        let e_c = clamp01(e);
        let r_c = clamp01(r);

        KerScore {
            k: k_c,
            e: e_c,
            r: r_c,
        }
    }

    /// Canonical scalar KER: s = k * e - r.
    /// This value is used as the kriging variable.
    pub fn scalar(&self) -> f64 {
        self.k * self.e - self.r
    }

    /// Reconstruct a triad from a scalar s and local statistics.
    /// This helper keeps the triad consistent with s by:
    /// - choosing k and e near local means,
    /// - adjusting r = k * e - s.
    pub fn from_scalar_with_means(s: f64, k_mean: f64, e_mean: f64) -> KerScore {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        let k_c = clamp01(k_mean);
        let e_c = clamp01(e_mean);
        let mut r_c = k_c * e_c - s;

        if r_c < 0.0 {
            r_c = 0.0;
        } else if r_c > 1.0 {
            r_c = 1.0;
        }

        KerScore {
            k: k_c,
            e: e_c,
            r: r_c,
        }
    }
}

/// 2D position of a hex anchor in a projected coordinate system.
/// This can be derived from Phoenix hex registry or any other grid.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct HexCoord {
    pub x: f64,
    pub y: f64,
}

/// A single neighbour sample used in kriging.
/// Contains position and scalar KER score s = k * e - r.
#[derive(Debug, Clone)]
pub struct HexKerSample {
    pub coord: HexCoord,
    pub ker: KerScore,
}

/// Simple isotropic variogram model for scalar KER.
/// This implementation uses an exponential model:
/// gamma(h) = nugget + sill * (1.0 - exp(-h / range))
#[derive(Debug, Clone, Copy)]
pub struct VariogramModel {
    pub nugget: f64,
    pub sill: f64,
    pub range: f64,
}

impl VariogramModel {
    pub fn gamma(&self, h: f64) -> f64 {
        if h <= 0.0 {
            return 0.0;
        }
        let r = if self.range <= 0.0 { 1.0 } else { self.range };
        self.nugget + self.sill * (1.0 - (-h / r).exp())
    }
}

/// Result of the kriging interpolation for the target hex.
#[derive(Debug, Clone)]
pub struct KrigingResult {
    pub interpolated_scalar: f64,
    pub interpolated_ker: KerScore,
    pub weights: Vec<f64>,
}

/// Compute Euclidean distance between two hex coordinates.
fn distance(a: HexCoord, b: HexCoord) -> f64 {
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    (dx * dx + dy * dy).sqrt()
}

/// Constrained ordinary kriging with nonnegative weights that sum to one.
/// This function:
/// - builds the kriging system using the supplied variogram,
/// - solves for unconstrained weights via a robust solver,
/// - projects weights into the simplex (lambda_i >= 0, sum lambda_i = 1),
/// - uses the projected weights to interpolate the scalar s,
/// - reconstructs a KER triad using local neighbour means.
///
/// This design preserves:
/// - positivity of s when all neighbours have positive s,
/// - compatibility with KER triad semantics.
pub fn constrained_ordinary_kriging(
    neighbours: &[HexKerSample],
    target_coord: HexCoord,
    variogram: VariogramModel,
) -> Option<KrigingResult> {
    let n = neighbours.len();
    if n == 0 {
        return None;
    }

    if n == 1 {
        let s = neighbours[0].ker.scalar();
        let k_mean = neighbours[0].ker.k;
        let e_mean = neighbours[0].ker.e;
        let interpolated_ker = KerScore::from_scalar_with_means(s, k_mean, e_mean);

        return Some(KrigingResult {
            interpolated_scalar: s,
            interpolated_ker,
            weights: vec![1.0],
        });
    }

    let size = n + 1;
    let mut a = vec![0.0f64; size * size];
    let mut b = vec![0.0f64; size];

    for i in 0..n {
        for j in 0..n {
            let h = distance(neighbours[i].coord, neighbours[j].coord);
            a[i * size + j] = variogram.gamma(h);
        }
        a[i * size + n] = 1.0;
        a[n * size + i] = 1.0;

        let h0 = distance(neighbours[i].coord, target_coord);
        b[i] = variogram.gamma(h0);
    }

    a[n * size + n] = 0.0;
    b[n] = 1.0;

    let x = solve_linear_system(&a, &b, size)?;
    let lambda_unconstrained = x[..n].to_vec();

    let lambda_projected = project_to_simplex(&lambda_unconstrained);
    let mut s_interp = 0.0;
    let mut k_sum = 0.0;
    let mut e_sum = 0.0;

    for (w, sample) in lambda_projected.iter().zip(neighbours.iter()) {
        let s_i = sample.ker.scalar();
        s_interp += w * s_i;
        k_sum += w * sample.ker.k;
        e_sum += w * sample.ker.e;
    }

    let k_mean = k_sum;
    let e_mean = e_sum;
    let interpolated_ker = KerScore::from_scalar_with_means(s_interp, k_mean, e_mean);

    Some(KrigingResult {
        interpolated_scalar: s_interp,
        interpolated_ker,
        weights: lambda_projected,
    })
}

/// Solve A x = b for x using a simple, numerically guarded
/// Gaussian elimination with partial pivoting.
/// A is stored as row-major, of size n x n.
fn solve_linear_system(a: &[f64], b: &[f64], n: usize) -> Option<Vec<f64>> {
    let mut a_mat = a.to_vec();
    let mut rhs = b.to_vec();

    for k in 0..n {
        let mut max_row = k;
        let mut max_val = a_mat[k * n + k].abs();

        for i in (k + 1)..n {
            let val = a_mat[i * n + k].abs();
            if val > max_val {
                max_val = val;
                max_row = i;
            }
        }

        if max_val < f64::EPSILON {
            return None;
        }

        if max_row != k {
            for j in 0..n {
                a_mat.swap(k * n + j, max_row * n + j);
            }
            rhs.swap(k, max_row);
        }

        let pivot = a_mat[k * n + k];
        for j in k..n {
            a_mat[k * n + j] /= pivot;
        }
        rhs[k] /= pivot;

        for i in 0..n {
            if i == k {
                continue;
            }
            let factor = a_mat[i * n + k];
            for j in k..n {
                a_mat[i * n + j] -= factor * a_mat[k * n + j];
            }
            rhs[i] -= factor * rhs[k];
        }
    }

    Some(rhs)
}

/// Project an arbitrary weight vector into the probability simplex:
/// lambda_i >= 0, sum lambda_i = 1.
/// Implements the standard Euclidean projection.
fn project_to_simplex(lambda: &[f64]) -> Vec<f64> {
    let n = lambda.len();
    let mut u = lambda.to_vec();
    u.sort_by(|a, b| b.partial_cmp(a).unwrap_or(std::cmp::Ordering::Equal));

    let mut sum = 0.0;
    let mut rho = -1;

    for (i, &u_i) in u.iter().enumerate() {
        sum += u_i;
        let t = (sum - 1.0) / ((i + 1) as f64);
        if u_i - t > 0.0 {
            rho = i as i32;
        }
    }

    if rho == -1 {
        return vec![1.0 / (n as f64); n];
    }

    sum = 0.0;
    for i in 0..=(rho as usize) {
        sum += u[i];
    }

    let theta = (sum - 1.0) / ((rho + 1) as f64);
    let mut projected = Vec::with_capacity(n);

    for &v in lambda.iter() {
        let val = v - theta;
        if val < 0.0 {
            projected.push(0.0);
        } else {
            projected.push(val);
        }
    }

    let s: f64 = projected.iter().sum();
    if s <= 0.0 {
        return vec![1.0 / (n as f64); n];
    }
    for v in projected.iter_mut() {
        *v /= s;
    }

    projected
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scalar_positive_preservation() {
        let neighbours = vec![
            HexKerSample {
                coord: HexCoord { x: 0.0, y: 0.0 },
                ker: KerScore::new(0.8, 0.9, 0.1),
            },
            HexKerSample {
                coord: HexCoord { x: 1.0, y: 0.0 },
                ker: KerScore::new(0.7, 0.85, 0.15),
            },
            HexKerSample {
                coord: HexCoord { x: 0.5, y: 0.8 },
                ker: KerScore::new(0.9, 0.95, 0.2),
            },
        ];

        let target = HexCoord { x: 0.4, y: 0.3 };
        let variogram = VariogramModel {
            nugget: 0.01,
            sill: 0.5,
            range: 1.5,
        };

        let result = constrained_ordinary_kriging(&neighbours, target, variogram).unwrap();
        assert!(result.interpolated_scalar > 0.0);
        assert!(result.interpolated_scalar <= 1.0);
    }

    #[test]
    fn test_simplex_projection() {
        let lambda = vec![0.5, -0.2, 1.3];
        let projected = project_to_simplex(&lambda);
        let sum: f64 = projected.iter().sum();
        assert!((sum - 1.0).abs() < 1e-9);
        for v in projected {
            assert!(v >= 0.0);
        }
    }
}
