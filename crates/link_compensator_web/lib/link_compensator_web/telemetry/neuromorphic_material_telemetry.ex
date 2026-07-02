defmodule LinkCompensatorWeb.NeuromorphicMaterialTelemetry do
  require Logger

  @event_name [:neuromorphic, :material, :band]

  def emit(profile, caps) do
    measurements = %{
      eco_impact_band: profile.eco_impact_band,
      is_compensated: profile.is_compensated
    }

    metadata = %{
      profile_id: profile.profile_id,
      paper_id: profile.paper_id,
      device_label: profile.device_label,
      material_system: profile.material_system,
      veco_cap: caps.veco_cap,
      evidence_hex: profile.evidence_hex
    }

    :telemetry.execute(@event_name, measurements, metadata)
  end
end
