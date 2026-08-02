// Path suggestion:
// src/urban_climate/evaluation/urban_climate_model_evaluation_kani.rs

#[cfg(kani)]
mod kani_harness {
    use super::{
        UrbanClimateModelComponent,
        UrbanClimateModelEvaluationEnvelope,
        EligibilityVerdict,
        enforce_urban_climate_model_eligibility,
        MIN_KER_SCORE,
        MIN_ROH_SCORE,
        MIN_BIODIVERSITY_SCORE,
        MIN_ECO_PLANES_SCORE,
        MIN_URBAN_CORRIDORS_SCORE,
        MIN_STREAMING_SLA_SCORE,
        MIN_NEURORIGHTS_SCORE,
    };

    // Helper to construct a bounded component for Kani exploration.
    fn arbitrary_component() -> UrbanClimateModelComponent {
        // kani::any() yields arbitrary values; we clamp them to 0..5.
        let mut c = UrbanClimateModelComponent {
            id: kani::any(),
            kind: kani::any(),
            description: kani::any(),

            ker_score: kani::any(),
            roh_score: kani::any(),
            biodiversity_score: kani::any(),
            ecological_planes_score: kani::any(),
            urban_corridors_score: kani::any(),
            streaming_sla_score: kani::any(),
            neurorights_score: kani::any(),

            governance_ref: kani::any(),
            ecology_ref: kani::any(),
            notes: kani::any(),
        };

        // Clamp scores to 0..5
        let clamp = |x: f32| -> f32 {
            if x < 0.0 { 0.0 } else if x > 5.0 { 5.0 } else { x }
        };

        c.ker_score               = clamp(c.ker_score);
        c.roh_score               = clamp(c.roh_score);
        c.biodiversity_score      = clamp(c.biodiversity_score);
        c.ecological_planes_score = clamp(c.ecological_planes_score);
        c.urban_corridors_score   = clamp(c.urban_corridors_score);
        c.streaming_sla_score     = clamp(c.streaming_sla_score);
        c.neurorights_score       = clamp(c.neurorights_score);

        c
    }

    #[kani::proof]
    fn deployment_eligibility_respects_min_scores_invariant() {
        let c = arbitrary_component();

        let env = UrbanClimateModelEvaluationEnvelope {
            envelope_id: kani::any(),
            region_context: kani::any(),
            components: vec![c.clone()],
            created_at_utc: "2026-08-02T00:00:00Z".to_string(),
        };

        let decisions = enforce_urban_climate_model_eligibility(&env);
        assert!(decisions.len() == 1);

        let d = &decisions[0];

        // Property 1: If all scores meet or exceed minima, verdict must be Eligible.
        if c.ker_score             >= MIN_KER_SCORE &&
           c.roh_score             >= MIN_ROH_SCORE &&
           c.biodiversity_score    >= MIN_BIODIVERSITY_SCORE &&
           c.ecological_planes_score >= MIN_ECO_PLANES_SCORE &&
           c.urban_corridors_score >= MIN_URBAN_CORRIDORS_SCORE &&
           c.streaming_sla_score   >= MIN_STREAMING_SLA_SCORE &&
           c.neurorights_score     >= MIN_NEURORIGHTS_SCORE
        {
            assert!(d.verdict == EligibilityVerdict::Eligible);
        }

        // Property 2: If any score is below minima, verdict must be Ineligible.
        if c.ker_score             < MIN_KER_SCORE ||
           c.roh_score             < MIN_ROH_SCORE ||
           c.biodiversity_score    < MIN_BIODIVERSITY_SCORE ||
           c.ecological_planes_score < MIN_ECO_PLANES_SCORE ||
           c.urban_corridors_score < MIN_URBAN_CORRIDORS_SCORE ||
           c.streaming_sla_score   < MIN_STREAMING_SLA_SCORE ||
           c.neurorights_score     < MIN_NEURORIGHTS_SCORE
        {
            assert!(d.verdict == EligibilityVerdict::Ineligible);
        }
    }
}
