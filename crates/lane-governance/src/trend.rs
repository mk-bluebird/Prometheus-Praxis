use crate::models::LaneEvidencePoint;

pub fn compute_trend(points: &[LaneEvidencePoint]) -> f64 {
    if points.len() < 2 {
        return 0.0;
    }

    let n = points.len() as f64;
    let mean_t =
        points.iter().map(|p| p.timestamp).sum::<f64>() / n;
    let mean_v =
        points.iter().map(|p| p.residual).sum::<f64>() / n;

    let mut num = 0.0;
    let mut den = 0.0;

    for p in points {
        let dt = p.timestamp - mean_t;
        let dv = p.residual - mean_v;
        num += dt * dv;
        den += dt * dt;
    }

    if den == 0.0 {
        0.0
    } else {
        num / den
    }
}
