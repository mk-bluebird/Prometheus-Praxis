// src/browser/duties/pfas-kriging.js
// Ordinary kriging for PFAS toxicity risk r_tox on hex-anchors.
// Provides prediction and normalized kriging variance as r_uncertainty.

class PFASKriging {
  constructor(variogramParams, options) {
    const defaults = {
      // semivariogram parameters: nugget c0, sill c, range a, type
      type: "exponential", // "exponential" or "spherical"
      c0: 0.0,
      c: 1.0,
      a: 1.0,
      // normalization for r_uncertainty
      maxVariance: 1.0
    };
    this.config = { ...defaults, ...variogramParams, ...options };
  }

  // Semivariogram model gamma(h).
  gamma(h) {
    const { type, c0, c, a } = this.config;
    const dist = Math.max(h, 0.0);
    if (type === "spherical") {
      if (dist >= a) {
        return c0 + c;
      }
      const ratio = dist / a;
      return c0 + c * (1.5 * ratio - 0.5 * ratio * ratio * ratio);
    }
    // default: exponential
    return c0 + c * (1.0 - Math.exp(-dist / a));
  }

  // Euclidean distance between two anchors (x,y).
  distance(p, q) {
    const dx = p.x - q.x;
    const dy = p.y - q.y;
    return Math.sqrt(dx * dx + dy * dy);
  }

  // Ordinary kriging prediction at location target { x, y }.
  // anchors: array of { x, y, rtox } with rtox in [0,1].
  predictAt(target, anchors) {
    const n = anchors.length;
    if (n === 0) {
      throw new Error("PFASKriging.predictAt requires at least one anchor.");
    }

    // Build A matrix (n+1 x n+1) and b vector (n+1).
    const size = n + 1;
    const A = new Array(size);
    for (let i = 0; i < size; i++) {
      A[i] = new Array(size).fill(0.0);
    }
    const b = new Array(size).fill(0.0);

    // Fill semivariogram entries.
    for (let i = 0; i < n; i++) {
      for (let j = 0; j < n; j++) {
        const h = this.distance(anchors[i], anchors[j]);
        A[i][j] = this.gamma(h);
      }
      A[i][n] = 1.0; // lambda sum constraint
      A[n][i] = 1.0;
      const h0 = this.distance(anchors[i], target);
      b[i] = this.gamma(h0);
    }
    A[n][n] = 0.0;
    b[n] = 1.0;

    // Solve A * x = b using simple Gaussian elimination.
    const x = this._gaussianSolve(A, b);
    const lambdas = x.slice(0, n);
    const mu = x[n];

    // Kriging estimate r_tox at target.
    let rtoxHat = 0.0;
    for (let i = 0; i < n; i++) {
      rtoxHat += lambdas[i] * anchors[i].rtox;
    }
    // Clamp to [0,1].
    rtoxHat = Math.max(0.0, Math.min(1.0, rtoxHat));

    // Kriging variance sigma_K^2 = sum_i lambda_i * gamma_{i0} + mu.
    let variance = 0.0;
    for (let i = 0; i < n; i++) {
      const h0 = this.distance(anchors[i], target);
      variance += lambdas[i] * this.gamma(h0);
    }
    variance += mu;

    // Normalize to r_uncertainty in [0,1].
    const rUnc = Math.max(
      0.0,
      Math.min(1.0, variance / Math.max(this.config.maxVariance, 1e-9))
    );

    return {
      r_tox: rtoxHat,
      r_uncertainty: rUnc,
      variance,
      lambdas
    };
  }

  // Basic Gaussian elimination solver for small systems.
  _gaussianSolve(A, b) {
    const n = A.length;
    // Augment matrix with b.
    for (let i = 0; i < n; i++) {
      A[i].push(b[i]);
    }
    // Forward elimination.
    for (let k = 0; k < n; k++) {
      // Pivot selection (no row swaps for simplicity, assume well-conditioned).
      const pivot = A[k][k] === 0 ? 1e-9 : A[k][k];
      for (let i = k + 1; i < n; i++) {
        const factor = A[i][k] / pivot;
        for (let j = k; j <= n; j++) {
          A[i][j] -= factor * A[k][j];
        }
      }
    }
    // Back substitution.
    const x = new Array(n).fill(0.0);
    for (let i = n - 1; i >= 0; i--) {
      let sum = A[i][n];
      for (let j = i + 1; j < n; j++) {
        sum -= A[i][j] * x[j];
      }
      const denom = A[i][i] === 0 ? 1e-9 : A[i][i];
      x[i] = sum / denom;
    }
    return x;
  }
}

export default PFASKriging;
