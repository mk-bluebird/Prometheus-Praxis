<!-- filename: docs/cyboquatic/CyboquaticRestorationSurface2026v1.md
     repo: mk-bluebird/eco_restoration_shard
     destination: Eco-Fort/docs/cyboquatic/CyboquaticRestorationSurface2026v1.md -->

# CyboquaticRestorationSurface2026v1

## Purpose

- Represent restoration radius, mass removal, and bio-karma contributions for Cyboquatic nodes and basins.
- Provide restorationok as a policy-aligned flag used by lane and placement logic.
- Expose gwriskmax for governance to combine restoration gains with residual risk.

## Governance and invariants

- restorationradiusm and restorationradiushours measure how far and for how long restoration effects extend around the node.
- deltamasswindowkg and deltakarmawindow quantify physical and karmic restoration over the window.
- restorationok must only be set to 1 when restoration metrics meet or exceed the current corridor thresholds and do not worsen non-offsettable planes.
- signingdid and evidencehex bind each row to its generating kernel and steward.

## Query patterns

- Select nodes where restorationok = 1 and restorationradiusm exceeds a minimum for scheduling.
- Identify nodes with high restorationradiusm but unacceptable gwriskmax for further investigation.
