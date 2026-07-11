# Prometheus‑Praxis Architecture

## 1. Purpose and Bounded Context

Prometheus‑Praxis is the ecosafety monitoring backbone for EcoNet and Cyboquatic deployments. Its sole responsibility is to collect, store, and expose metrics that support:

- Lyapunov residuals \(V_t\) over ecosafety risk planes.
- KER (Knowledge, Eco‑impact, Risk) windows.
- Evidence‑grade qpudatashards for downstream governance.

Prometheus‑Praxis is **not** a generic Prometheus tutorial or a random collection of manifests. It is a defined, production‑grade monitoring stack for:

- Environmental nodes (water, soil, air).
- Energy nodes (Smartflower, energy banks).
- Cyber and data‑quality planes.

This document defines the architecture, service boundaries, and deployment model.

## 2. High‑Level Architecture

The stack is decomposed into four primary components:

- `prometheus-core`:
  - Time‑series database (TSDB) and rule engine.
  - Scrapes exporters and stores metrics.
- `alertmanager-core`:
  - Receives alerts from Prometheus.
  - Routes notifications to ecosafety operators and automation hooks.
- `eco-grafana`:
  - Dashboards for Lyapunov residuals, KER scores, and corridor status.
- `eco-shard-writer` (optional extension):
  - Bridges metrics into qpudatashards (CSV/ALN) for EcoNet governance.

Each component runs as an independent service with a clearly defined network interface and lifecycle.

## 3. Service Boundaries and Responsibilities

### 3.1 prometheus-core

- Scrapes:
  - `node_exporter` for host metrics.
  - Application exporters (water quality, Smartflower, energy banks).
  - Internal ecosafety exporters (rcalib, rsigma, KER, Vt).
- Stores:
  - Time‑series in a local TSDB with PVC‑backed storage.
- Evaluates:
  - Recording rules (e.g., Vt, JLF components).
  - Alerting rules (e.g., high Vt, corridor violations).

Prometheus does not expose a public UI directly. Access is mediated by the ingress layer and RBAC.

### 3.2 alertmanager-core

- Listens for alerts from Prometheus.
- Routes to:
  - Email, chat, or incident systems.
  - Optional eco‑automation hooks (e.g., derating or safe‑state triggers via separate control planes).
- Runs as a stateless deployment; configuration is supplied via ConfigMaps and Secrets.

Alertmanager has no direct access to environmental controllers to preserve separation of concerns.

### 3.3 eco-grafana

- Dashboards:
  - KER windows (K, E, R).
  - Lyapunov residual trends.
  - Corridor status per plane (rcarbon, rmaterials, rhydraulic, rbiology, rcalib, rsigma).
- Integration:
  - Uses Prometheus as the primary data source.
  - Optionally integrates with long‑term backends (Thanos, Cortex) via separate data sources.

Access is authenticated via an identity provider (OIDC) or local credentials behind an ingress.

### 3.4 eco-shard-writer (future extension)

- Pulls:
  - Prometheus metrics over HTTP API.
- Writes:
  - qpudatashards in CSV or ALN format to a configured storage.
- Enforces:
  - NoCorridorNoBuild and SafeStepLyapunov invariants on exported windows before marking them deployable.

This service is optional and can run as a batch or scheduled job.

## 4. Deployment Model

Prometheus‑Praxis supports two deployment modes:

- Local Docker Compose:
  - For development and small‑scale labs.
  - Single Prometheus / Alertmanager / Grafana instance.
- Kubernetes:
  - For production and high‑availability.
  - Supports stateful Prometheus with PersistentVolumeClaims.
  - Supports ingress, mTLS, and horizontal scalability.

All production deployments should use Kubernetes.

## 5. State Management

Prometheus is stateful:

- TSDB data is stored on a PersistentVolumeClaim.
- Retention policies are explicitly configured.
- Backup and disaster recovery:
  - TSDB snapshots can be scheduled.
  - Optional integration with remote storage (e.g., object storage via Thanos) for long‑term retention.

Alertmanager and Grafana are treated as stateless, with their configuration and dashboards stored as code (ConfigMaps and provisioning files).

## 6. Networking and Security

- Ingress:
  - A single ingress controller (e.g., NGINX Ingress) terminates TLS at the edge.
  - Internal traffic uses cluster‑internal DNS names and mTLS if a service mesh is enabled.
- Zero‑trust:
  - No service is exposed directly to the internet.
  - Inter‑service traffic uses authenticated and encrypted channels where possible.
- Authentication:
  - Grafana uses OIDC or local users behind the ingress.
  - Prometheus UI access is restricted to operator roles.
- Authorization:
  - Kubernetes RBAC restricts access to namespaces and resources.
  - Prometheus endpoints are restricted via ingress rules and firewalls.

## 7. Observability of the Monitoring Stack

Prometheus‑Praxis monitors itself:

- Self‑metrics:
  - TSDB health (cardinality, blocks, compactions).
  - Scrape errors and rule evaluation latency.
- Alerts:
  - Firing when the monitoring plane loses scrape coverage or TSDB health degrades.
- Dashboards:
  - Dedicated panels for monitoring stack health (Prometheus, Alertmanager, Grafana, exporters).

This ensures KER and Vt computations are backed by a healthy telemetry plane.

## 8. Infrastructure as Code and GitOps

- All deployments are defined as code:
  - Dockerfiles.
  - Kubernetes manifests (Deployments, Services, ConfigMaps, Secrets, Ingress).
- CI/CD:
  - Linting of manifests.
  - Security scans of images.
  - Automated deployment via GitOps (Argo CD or Flux) in production environments.

No manual changes are made directly in production clusters; all changes flow through version‑controlled manifests.

## 9. Environmental and EcoNet Alignment

Prometheus‑Praxis is aligned with EcoNet and Cyboquatic corridors:

- Metrics:
  - Expose risk coordinates (rj) as labeled time‑series.
  - Expose KER and Vt as derived metrics.
- Governance:
  - Metric schemas are documented and stable.
  - Any new coordinate enters the Lyapunov residual via a non‑negative weight and a normalized 0–1 corridor.

This architecture ensures that monitoring data is trustworthy, stable, and ready for eco‑governance and external audits.
