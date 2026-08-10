// File: crates/ker-composition/src/workload_proof.rs
#![forbid(unsafe_code)]

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WorkloadKerFrame {
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub accepted: bool,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum KerProofError {
    NonFiniteValue,
    OutOfRange,
    AcceptedFrameViolatesKer,
}

pub fn ker_margin(frame: WorkloadKerFrame) -> Result<f64, KerProofError> {
    let values = [frame.ker_k, frame.ker_e, frame.ker_r];
    if values.iter().any(|value| !value.is_finite()) {
        return Err(KerProofError::NonFiniteValue);
    }
    if values.iter().any(|value| !(0.0..=1.0).contains(value)) {
        return Err(KerProofError::OutOfRange);
    }
    Ok(frame.ker_k * frame.ker_e - frame.ker_r)
}

pub fn prove_accepted_frame(frame: WorkloadKerFrame) -> Result<(), KerProofError> {
    let margin = ker_margin(frame)?;
    if frame.accepted && margin <= 0.0 {
        return Err(KerProofError::AcceptedFrameViolatesKer);
    }
    Ok(())
}

pub fn prove_all_accepted(frames: &[WorkloadKerFrame]) -> Result<(), KerProofError> {
    for frame in frames {
        prove_accepted_frame(*frame)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn accepted_frame_has_strictly_positive_ker_margin() {
        let frame = WorkloadKerFrame {
            ker_k: 0.93,
            ker_e: 0.90,
            ker_r: 0.12,
            accepted: true,
        };
        assert!(ker_margin(frame).unwrap() > 0.0);
        assert_eq!(prove_accepted_frame(frame), Ok(()));
    }

    #[test]
    fn rejected_frame_may_have_nonpositive_ker_margin() {
        let frame = WorkloadKerFrame {
            ker_k: 0.50,
            ker_e: 0.50,
            ker_r: 0.30,
            accepted: false,
        };
        assert_eq!(prove_accepted_frame(frame), Ok(()));
    }

    #[test]
    fn invalid_accepted_frame_fails_the_proof() {
        let frame = WorkloadKerFrame {
            ker_k: 0.50,
            ker_e: 0.50,
            ker_r: 0.25,
            accepted: true,
        };
        assert_eq!(
            prove_accepted_frame(frame),
            Err(KerProofError::AcceptedFrameViolatesKer)
        );
    }

    #[test]
    fn collection_proof_stops_on_invalid_accepted_frame() {
        let frames = [
            WorkloadKerFrame {
                ker_k: 0.94,
                ker_e: 0.91,
                ker_r: 0.12,
                accepted: true,
            },
            WorkloadKerFrame {
                ker_k: 0.50,
                ker_e: 0.50,
                ker_r: 0.25,
                accepted: true,
            },
        ];
        assert_eq!(
            prove_all_accepted(&frames),
            Err(KerProofError::AcceptedFrameViolatesKer)
        );
    }
}
