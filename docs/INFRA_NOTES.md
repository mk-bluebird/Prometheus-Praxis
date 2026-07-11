# Prometheus‑Praxis Infrastructure Notes

This document captures operational guidance for running Prometheus‑Praxis in production and aligning it with ecosafety and regulatory requirements.

## 1. Environments

Recommended environments:

- `dev`:
  - Single‑node Kubernetes or Docker Compose.
  - Short retention (7–14 days).
- `staging`:
  - Kubernetes cluster with similar sizing to production.
  - Retention 30–60 days.
- `prod`:
  - Kubernetes cluster with HA control plane.
  - Prometheus STL plus optional remote storage.

Each environment uses separate namespaces and distinct external labels on metrics (environment, region).

## 2. Persistent Storage

Prometheus TSDB:

- Use SSD‑backed PersistentVolumes with low latency.
- Size according to:
  - metric cardinality,
  - retention time,
  - ingest rate.
- Example retention:
  - `90d` time,
  - `50GB` size cap.

Coordinate with eco‑governance requirements for how long KER/Vt histories must be retained for audits.

## 3. Backup and Disaster Recovery

Two levels of protection:

- TSDB snapshots:
  - Schedule regular snapshots of the Prometheus data directory.
  - Store snapshots in object storage with checksums.
- Remote storage (optional):
  - Use Thanos or Cortex to replicate data to object storage.
  - This enables long‑term, cost‑effective retention.

Simulate restoring from backup and verify KER/Vt continuity.

## 4. Security and Access Control

- Ingress security:
  - Terminate TLS using a trusted certificate.
  - Restrict hostnames to internal DNS.
- Authentication:
  - Integrate Grafana with an identity provider via OIDC.
  - Limit Prometheus UI access to operators.
- Authorization:
  - Use Kubernetes RBAC to restrict namespaces and resources.
  - Avoid cluster‑admin roles for routine operations.
- Secrets:
  - Do not commit secrets to Git.
  - Use Kubernetes Secrets or a dedicated secret manager.

## 5. Observability

Prometheus‑Praxis must be observable:

- Monitor:
  - Prometheus self‑metrics.
  - Alertmanager status.
  - Grafana uptime and response times.
- Log aggregation:
  - Route logs to a central logging system.
  - Use structured logging where supported.

Set up dashboards and alerts for the monitoring plane itself to avoid blind spots.

## 6. CI/CD and GitOps

Implement a pipeline that:

- Validates:
  - YAML syntax.
  - Kubernetes best practices.
- Scans:
  - Container images for vulnerabilities.
- Deploys:
  - Using GitOps tools.
  - No manual `kubectl apply` in production.

Each change to Prometheus‑Praxis should be reviewed, tested in staging, and then promoted to production.

## 7. Alignment with EcoNet and Cyboquatic Corridors

- Metric naming:
  - Use consistent naming for risk coordinates and KER values.
- Lyapunov residuals:
  - Implement recording rules to compute Vt.
- KER windows:
  - Implement rules and dashboards to display KER over time.

Ensure that any change to metrics or rules is reflected in qpudatashard schemas and EcoNet documentation.
