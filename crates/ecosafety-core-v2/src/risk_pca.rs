// filename: crates/ecosafety-core-v2/src/risk_pca.rs

#![forbid(unsafe_code)]

use ndarray::{Array2, Array1};
use linfa::traits::Fit;
use linfa_reduction::Pca;

/// Build a 2D PCA embedding for high-dimensional risk vectors.
pub struct RiskPcaEmbedding {
    pub pca: Pca<f64>,
    pub lyap_weights: Array1<f64>,
}

impl RiskPcaEmbedding {
    pub fn fit(
        risk_vectors: Array2<f64>,   // shape: [n_hex, d_planes]
        lyap_weights: Array1<f64>,   // length d_planes, w_j
    ) -> Self {
        // Optionally augment risk_vectors with Lyapunov residual V as an extra feature
        let n_hex = risk_vectors.nrows();
        let d_planes = risk_vectors.ncols();

        let mut augmented = Array2::<f64>::zeros((n_hex, d_planes + 1));
        augmented
            .slice_mut(ndarray::s![.., 0..d_planes])
            .assign(&risk_vectors);

        // Compute Lyapunov residual V(h) and store as last column
        for h in 0..n_hex {
            let r = risk_vectors.slice(ndarray::s![h, ..]);
            let mut v = 0.0;
            for j in 0..d_planes {
                let rj = r[j];
                let wj = lyap_weights[j];
                v += wj * rj * rj;
            }
            augmented[[h, d_planes]] = v;
        }

        // Fit PCA to augmented data, target 2D embedding
        let dataset = linfa::DatasetBase::from(augmented);
        let pca = Pca::params(2)
            .fit(&dataset)
            .expect("PCA fitting failed");

        RiskPcaEmbedding { pca, lyap_weights }
    }

    pub fn transform(&self, risk_vectors: Array2<f64>) -> Array2<f64> {
        let n_hex = risk_vectors.nrows();
        let d_planes = risk_vectors.ncols();
        let mut augmented = Array2::<f64>::zeros((n_hex, d_planes + 1));

        augmented
            .slice_mut(ndarray::s![.., 0..d_planes])
            .assign(&risk_vectors);

        for h in 0..n_hex {
            let r = risk_vectors.slice(ndarray::s![h, ..]);
            let mut v = 0.0;
            for j in 0..d_planes {
                let rj = r[j];
                let wj = self.lyap_weights[j];
                v += wj * rj * rj;
            }
            augmented[[h, d_planes]] = v;
        }

        self.pca.transform(&augmented)
    }
}
