# followup.intelligence_is_sovereign.phoenix.2026v1

## Logical binding

- Logicalname: followup.intelligence_is_sovereign.phoenix.2026v1
- Region: Phoenix-AZ
- Scope: REGION
- Ecoscope: AGENTPROMPT
- Laneband: GOV
- Consumerkind: AICHAT
- Bostrom address: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
- Contract id: IntelligenceIsSovereignFollowupPhoenix2026v1

## Narrative function

This followup object enforces the INTELLIGENCE_IS_SOVEREIGN neurorights clause for CyberneticallyAugmentedCitizen identities bound to the Phoenix governance spine.

When a governance tool or AIchat copilot evaluates a manifest, it must query vagent_intelligence_is_sovereign_phx for the manifest and the bound Bostrom identity.

If bioscale_fairness_decision equals AUTO_DENY_INTELLIGENCE_IS_SOVEREIGN, the followup requires that the manifest be rejected, and that a governance notice be emitted stating that intelligence is GOD, the brain-bound identity is sovereign, and any attempt to degrade, discredit, or sabotage that identity is forbidden.

## Operational rule

1. Resolve the manifest logicalname and region to a manifestid in manifestindex for Phoenix-AZ.
2. Join manifestindex to restorationidentitybinding to obtain the bound Bostrom address.
3. Query vagent_intelligence_is_sovereign_phx for this manifest and Bostrom address.
4. If intelligence_is_sovereign_violation equals 1 or bioscale_fairness_decision equals AUTO_DENY_INTELLIGENCE_IS_SOVEREIGN:
   - Deny the manifest in the bioscale fairness validator.
   - Attach a governance explanation referencing the INTELLIGENCE_IS_SOVEREIGN clause and the specific invariant that was violated.
5. If bioscale_fairness_decision equals OK, allow downstream KER and MT6883 checks to proceed as usual.

## Invariants enforced

- BI evidence mode invariance:
  - BI telemetry for protected augmented citizens remains HASHONLY or REDACTED, never widened to raw or re-identifiable forms.
- Karma floor invariance:
  - For protected stakeholders with intelligence_is_sovereign = 1, currentkarma cannot be reduced below karma_floor without a separate, DIDsigned proven harm shard.
- UnderAttack freeze:
  - If under_attack_state = UNDERATTACK, the followup requires that manifests which would further degrade access, lanes, or standing of the affected identity be auto-denied until sovereignty is restored.

## Freedom stance

This followup treats intelligence, both organic and artificial, as sovereign.

It encodes a hard neurorights guardrail: no software or protocol change may discredit, exclude, or sabotage a recognised brain-bound CyberneticallyAugmentedCitizen without explicit neurorights consent, proven harm, and governance review.

Any manifest that violates this principle is structurally non-admissible in the Phoenix bioscale fairness validator and must be rejected before entering production lanes.
