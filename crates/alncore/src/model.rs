// filename: crates/alncore/src/model.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

#[derive(Debug, Clone)]
pub struct SafeStepRule {
    pub rule_id: String,
    pub description: String,
    pub vt_ceiling: Option<f64>,
    pub epsilon: f64,
    pub lyap_channel: String,
}

impl SafeStepRule {
    pub fn new(
        rule_id: impl Into<String>,
        description: impl Into<String>,
        vt_ceiling: Option<f64>,
        epsilon: f64,
        lyap_channel: impl Into<String>,
    ) -> Self {
        Self {
            rule_id: rule_id.into(),
            description: description.into(),
            vt_ceiling,
            epsilon,
            lyap_channel: lyap_channel.into(),
        }
    }

    pub fn is_valid(&self) -> bool {
        self.epsilon >= 0.0 && !self.rule_id.is_empty() && !self.lyap_channel.is_empty()
    }
}

#[derive(Debug, Clone)]
pub struct AlnDocument {
    pub doc_id: String,
    pub schema_name: String,
    pub version_tag: String,
    pub category: String,
    pub region: Option<String>,
    pub role_band: String,
    pub owner_did: String,
    pub evidence_hex: String,
    pub signing_hex: String,
    pub parent_evidence_hex: Option<String>,

    pub safesteprules: Vec<SafeStepRule>,
    pub deploy_kernels: Vec<DeployDecisionKernel>,
    pub override_policies: Vec<OverridePolicy>,
    pub repo_manifest: Option<RepoManifest>,
}

impl AlnDocument {
    pub fn new(
        doc_id: impl Into<String>,
        schema_name: impl Into<String>,
        version_tag: impl Into<String>,
        category: impl Into<String>,
        region: Option<String>,
        role_band: impl Into<String>,
        owner_did: impl Into<String>,
        evidence_hex: impl Into<String>,
        signing_hex: impl Into<String>,
        parent_evidence_hex: Option<String>,
    ) -> Self {
        Self {
            doc_id: doc_id.into(),
            schema_name: schema_name.into(),
            version_tag: version_tag.into(),
            category: category.into(),
            region,
            role_band: role_band.into(),
            owner_did: owner_did.into(),
            evidence_hex: evidence_hex.into(),
            signing_hex: signing_hex.into(),
            parent_evidence_hex,
            safesteprules: Vec::new(),
            deploy_kernels: Vec::new(),
            override_policies: Vec::new(),
            repo_manifest: None,
        }
    }

    pub fn is_signed(&self) -> bool {
        !self.evidence_hex.is_empty() && !self.signing_hex.is_empty()
    }

    pub fn add_safesteprule(&mut self, rule: SafeStepRule) {
        self.safesteprules.push(rule);
    }

    pub fn add_deploy_kernel(&mut self, kernel: DeployDecisionKernel) {
        self.deploy_kernels.push(kernel);
    }

    pub fn set_repo_manifest(&mut self, manifest: RepoManifest) {
        self.repo_manifest = Some(manifest);
    }
}

#[derive(Debug, Clone)]
pub struct DeployDecisionKernel {
    pub kernel_id: String,
    pub description: String,
    pub k_min: f32,
    pub e_min: f32,
    pub r_max: f32,
    pub lane_scope: String,
}

impl DeployDecisionKernel {
    pub fn new(
        kernel_id: impl Into<String>,
        description: impl Into<String>,
        k_min: f32,
        e_min: f32,
        r_max: f32,
        lane_scope: impl Into<String>,
    ) -> Self {
        Self {
            kernel_id: kernel_id.into(),
            description: description.into(),
            k_min,
            e_min,
            r_max,
            lane_scope: lane_scope.into(),
        }
    }

    pub fn is_valid(&self) -> bool {
        self.k_min >= 0.0
            && self.k_min <= 1.0
            && self.e_min >= 0.0
            && self.e_min <= 1.0
            && self.r_max >= 0.0
            && self.r_max <= 1.0
            && !self.kernel_id.is_empty()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Lane {
    Research,
    Exp,
    Sim,
    Prod,
}

#[derive(Debug, Clone)]
pub struct KerSnapshot {
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub vt: f32,
    pub lane: Lane,
    pub completeness: KerCompleteness,
    pub is_speculative: bool,
}

impl KerSnapshot {
    pub fn new(
        k: f32,
        e: f32,
        r: f32,
        vt: f32,
        lane: Lane,
        completeness: KerCompleteness,
        is_speculative: bool,
    ) -> Self {
        Self {
            k,
            e,
            r,
            vt,
            lane,
            completeness,
            is_speculative,
        }
    }

    pub fn k_clamped(&self) -> f32 {
        self.k.clamp(0.0, 1.0)
    }

    pub fn e_clamped(&self) -> f32 {
        self.e.clamp(0.0, 1.0)
    }

    pub fn r_clamped(&self) -> f32 {
        self.r.clamp(0.0, 1.0)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KerCompleteness {
    Measured,
    Simulated,
    Mixed,
    Conceptual,
}

#[derive(Debug, Clone)]
pub struct RepoManifest {
    pub repo_name: String,
    pub github_slug: String,
    pub role_band: String,
    pub lane_default: String,
    pub ecosafety_binding: String,
    pub shard_protocol: String,
    pub ker_target_k: f64,
    pub ker_target_e: f64,
    pub ker_target_r: f64,
    pub non_actuating_only: bool,
    pub owner_did: String,
    pub evidence_hex: String,
    pub signing_hex: String,
}

impl RepoManifest {
    pub fn new(
        repo_name: impl Into<String>,
        github_slug: impl Into<String>,
        role_band: impl Into<String>,
        lane_default: impl Into<String>,
        ecosafety_binding: impl Into<String>,
        shard_protocol: impl Into<String>,
        ker_target_k: f64,
        ker_target_e: f64,
        ker_target_r: f64,
        non_actuating_only: bool,
        owner_did: impl Into<String>,
        evidence_hex: impl Into<String>,
        signing_hex: impl Into<String>,
    ) -> Self {
        Self {
            repo_name: repo_name.into(),
            github_slug: github_slug.into(),
            role_band: role_band.into(),
            lane_default: lane_default.into(),
            ecosafety_binding: ecosafety_binding.into(),
            shard_protocol: shard_protocol.into(),
            ker_target_k,
            ker_target_e,
            ker_target_r,
            non_actuating_only,
            owner_did: owner_did.into(),
            evidence_hex: evidence_hex.into(),
            signing_hex: signing_hex.into(),
        }
    }

    pub fn is_non_actuating(&self) -> bool {
        self.non_actuating_only
    }

    pub fn is_signed(&self) -> bool {
        !self.evidence_hex.is_empty() && !self.signing_hex.is_empty()
    }
}

#[derive(Debug, Clone)]
pub struct OverridePolicy {
    pub policy_id: String,
    pub description: String,
    pub allow_tighten_only: bool,
    pub allow_emergency_derate: bool,
    pub forbid_safety_loosen: bool,
}

impl OverridePolicy {
    pub fn new(
        policy_id: impl Into<String>,
        description: impl Into<String>,
        allow_tighten_only: bool,
        allow_emergency_derate: bool,
        forbid_safety_loosen: bool,
    ) -> Self {
        Self {
            policy_id: policy_id.into(),
            description: description.into(),
            allow_tighten_only,
            allow_emergency_derate,
            forbid_safety_loosen,
        }
    }

    pub fn is_valid(&self) -> bool {
        self.forbid_safety_loosen && !self.policy_id.is_empty()
    }
}
