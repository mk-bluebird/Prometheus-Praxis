# Non-Penalty Incident Classification v1

## Purpose

This specification defines fact-based incident classes for protected systems, data boundaries, consent streams, and continuity-related events.

An incident classification is an intake and investigation label. It is not:

- A finding of fault
- A clinical diagnosis
- A legal determination
- A finding of intent
- A penalty
- A CyberRank mutation
- A change to a local commitment artifact
- A restriction of rights
- An access-control decision
- An action authorization
- A person-ranking input

## Event classes

### UnauthorizedAccess

Use when there is evidence of actual or attempted access, retrieval, query, export, transfer, or disclosure without documented authority, valid scope, or applicable consent.

```text
examples:
  - an unapproved principal queries a protected store
  - an API request attempts access beyond permitted scope
  - a service exports protected data without a valid purpose
  - a cross-stream access route is attempted without an approved exception
```

### ProhibitedInference

Use when a system is alleged or evidenced to have generated, attempted, requested, or transmitted an inference prohibited by policy from protected biological, neural, biometric, identity, or sovereignty-related data.

```text
examples:
  - identity inference from a receipt
  - person ranking from topology or artifact quality
  - clinical inference from a minimized provenance receipt
  - action authorization from biological observation
```

### PolicyViolation

Use when a documented technical, procedural, governance, privacy, retention, cryptographic, or consent requirement appears not to have been followed.

```text
examples:
  - noncanonical artifact encoding
  - missing threshold signature
  - expired signer attestation
  - prohibited receipt field accepted by a parser
  - unapproved cross-stream reference
  - missing reassessment record for a continuing restriction
```

### AmbiguousEvent

Use when an anomaly, report, or partial evidence exists but the available record does not support assignment to another class.

```text
examples:
  - incomplete logs
  - competing explanations
  - unverifiable reporter evidence
  - telemetry anomaly without a confirmed access event
  - possible linkage risk not yet reproduced
```

## Mandatory facts

Every record must include only the minimum facts needed for investigation:

```text
incident_id
event_class
classification_status
reported_at
observed_at_or_time_bucket
reporting_party_reference
system_component_reference
policy_or_consent_reference
evidence_commitments
scope_of_possible_exposure
containment_status
notice_status
review_status
resolution_status
retention_policy_reference
```

Raw biological data, raw waveform data, neurobiometric templates, unnecessary medical information, private keys, recovery material, or broad identity profiles must not be copied into the routine incident record.

## Classification status

```text
reported
triaged
under_review
substantiated
not_substantiated
resolved
```

A report remains an allegation until independently reviewed.

\[
\operatorname{ReportedIncident}(e)
\not\Rightarrow
\operatorname{SubstantiatedViolation}(e)
\]

\[
\operatorname{SubstantiatedViolation}(e)
\not\Rightarrow
\operatorname{AutomaticPenalty}(e)
\]

## Non-penalty invariant

For every incident \(e\):

\[
\operatorname{Classify}(e,c)
\Rightarrow
\operatorname{ProtectedState}_{after}
=
\operatorname{ProtectedState}_{before}
\]

where:

\[
\operatorname{ProtectedState}
=
\{
\operatorname{CyberRankTier2},
\operatorname{identity},
\operatorname{rights},
\operatorname{personhood},
\operatorname{clinicalStatus},
\operatorname{integrationStatus}
\}
\]

Therefore:

\[
\operatorname{IncidentClass}(e)
\not\Rightarrow
\operatorname{CyberRankMutation}
\]

\[
\operatorname{IncidentClass}(e)
\not\Rightarrow
\operatorname{RightsChange}
\]

\[
\operatorname{IncidentClass}(e)
\not\Rightarrow
\operatorname{ClinicalDecision}
\]

\[
\operatorname{IncidentClass}(e)
\not\Rightarrow
\operatorname{ActionAuthorization}
\]

## Proportionate response

The permitted initial response is limited to:

```text
preserve evidence
minimize further exposure
validate scope
notify through accessible channels where appropriate
offer correction or context
assign independent review
document resolution
repair a technical control where necessary
```

The record must not automatically create:

```text
punitive score
reputation score
access ban
identity downgrade
clinical label
rights restriction
device restriction
person-value assessment
```

## Due process

A person affected by an incident classification may:

```text
receive accessible notice
receive the allegation and applicable policy basis
review a minimally necessary evidence summary
submit correction or contextual explanation
designate an advocate or representative
request independent review
appeal a disputed classification or remedy
receive a written resolution record
```

## Retention

Incident evidence must be retained only for the documented investigation, remediation, audit, and legal-compliance purpose. Retention schedules, access grants, and disposal events must be logged.
