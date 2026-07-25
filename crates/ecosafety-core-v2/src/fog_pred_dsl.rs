// filename: crates/ecosafety-core-v2/src/fog_pred_dsl.rs

#![forbid(unsafe_code)]

/// Context type passed into all predicates (risk vector, bands, weights, etc.).
pub struct FogContext<'a> {
    pub risk: &'a crate::RiskVector,
    pub bands: &'a crate::CorridorBands,
    pub weights: &'a crate::LyapunovWeights,
    // ... other fields as needed
}

// Primitive scalar predicate: thermal risk scalar.
pub fn thermal_scalar(ctx: &FogContext) -> crate::Scalar {
    ctx.risk.r_thermal
}

/// Example boolean predicates, matching your earlier FOG functions.[file:14]
pub fn hydraulic_ok(ctx: &FogContext) -> bool {
    crate::fog_router::hydraulic_ok(ctx.risk, ctx.bands)
}

pub fn biodiv_ok(ctx: &FogContext) -> bool {
    crate::fog_router::biodiv_ok(ctx.risk, ctx.bands)
}

// Lyapunov_ok returns a struct; treat invariant_ok as the boolean.
pub fn lyapunov_ok(ctx: &FogContext) -> bool {
    let step = crate::LyapunovStep::new(1.0, 1.0, 0.0); // placeholder; supply real step externally
    let res = crate::fog_router::lyapunov_ok(ctx.weights, ctx.risk, ctx.risk, &step);
    res.invariant_ok
}

/// DSL macro: pred!( hydraulic_ok && (lyapunov_ok || thermal_scalar < 0.3) )
macro_rules! pred {
    // Base: identifier predicate -> function call on &ctx.
    ($ctx:ident, $ident:ident) => {
        $ident($ctx)
    };

    // Comparison: IDENT < NUMBER -> scalar function + threshold.
    ($ctx:ident, $ident:ident < $val:literal) => {
        thermal_scalar($ctx) < $val
    };

    // AND: left && right.
    ($ctx:ident, $left:tt && $right:tt) => {
        pred!($ctx, $left) && pred!($ctx, $right)
    };

    // OR: left || right.
    ($ctx:ident, $left:tt || $right:tt) => {
        pred!($ctx, $left) || pred!($ctx, $right)
    };

    // Parentheses: (expr).
    ($ctx:ident, ( $($inner:tt)+ )) => {
        pred!($ctx, $($inner)+)
    };
}

pub use pred;
