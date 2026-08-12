-- File: sql/ppx_ai_workload/solar_storage_feasible_budget.sql
-- Required table:
-- ppx_solar_forecast(observed_utc TEXT PRIMARY KEY, solar_kwh REAL,
--                    fixed_load_kwh REAL, power_capacity_kw REAL)
-- ppx_storage_config(storage_capacity_kwh REAL, initial_buffer_kwh REAL,
--                    reserve_kwh REAL, charge_efficiency REAL,
--                    discharge_efficiency REAL, max_discharge_kwh_per_hour REAL)

WITH RECURSIVE
ordered AS (
    SELECT
        observed_utc,
        solar_kwh,
        fixed_load_kwh,
        power_capacity_kw,
        ROW_NUMBER() OVER (ORDER BY observed_utc) AS hour_index
    FROM ppx_solar_forecast
    ORDER BY observed_utc
    LIMIT 24
),
budget(
    hour_index,
    observed_utc,
    solar_kwh,
    fixed_load_kwh,
    power_capacity_kw,
    storage_kwh,
    direct_surplus_kwh,
    feasible_ai_energy_kwh
) AS (
    SELECT
        o.hour_index,
        o.observed_utc,
        o.solar_kwh,
        o.fixed_load_kwh,
        o.power_capacity_kw,
        MIN(
            c.storage_capacity_kwh,
            MAX(
                0.0,
                c.initial_buffer_kwh +
                c.charge_efficiency * MAX(o.solar_kwh - o.fixed_load_kwh, 0.0) -
                MAX(o.fixed_load_kwh - o.solar_kwh, 0.0) / c.discharge_efficiency
            )
        ),
        MAX(o.solar_kwh - o.fixed_load_kwh, 0.0),
        MIN(
            o.power_capacity_kw,
            MAX(o.solar_kwh - o.fixed_load_kwh, 0.0) +
            MIN(
                c.max_discharge_kwh_per_hour,
                MAX(0.0, c.initial_buffer_kwh - c.reserve_kwh)
            )
        )
    FROM ordered AS o
    CROSS JOIN ppx_storage_config AS c
    WHERE o.hour_index = 1

    UNION ALL

    SELECT
        o.hour_index,
        o.observed_utc,
        o.solar_kwh,
        o.fixed_load_kwh,
        o.power_capacity_kw,
        MIN(
            c.storage_capacity_kwh,
            MAX(
                0.0,
                b.storage_kwh +
                c.charge_efficiency * MAX(o.solar_kwh - o.fixed_load_kwh, 0.0) -
                MAX(o.fixed_load_kwh - o.solar_kwh, 0.0) / c.discharge_efficiency
            )
        ),
        MAX(o.solar_kwh - o.fixed_load_kwh, 0.0),
        MIN(
            o.power_capacity_kw,
            MAX(o.solar_kwh - o.fixed_load_kwh, 0.0) +
            MIN(
                c.max_discharge_kwh_per_hour,
                MAX(0.0, b.storage_kwh - c.reserve_kwh)
            )
        )
    FROM budget AS b
    JOIN ordered AS o ON o.hour_index = b.hour_index + 1
    CROSS JOIN ppx_storage_config AS c
)
SELECT
    observed_utc,
    solar_kwh,
    fixed_load_kwh,
    storage_kwh AS storage_buffer_kwh,
    direct_surplus_kwh,
    feasible_ai_energy_kwh,
    MIN(power_capacity_kw, feasible_ai_energy_kwh) AS power_limited_feasible_kwh
FROM budget
ORDER BY hour_index;
