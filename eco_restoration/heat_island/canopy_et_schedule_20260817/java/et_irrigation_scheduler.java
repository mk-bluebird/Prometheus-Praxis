public final class EtIrrigationScheduler {
    private EtIrrigationScheduler() {}

    private record Result(
        double etcMm,
        double plantDemandLiters,
        double usableCanalSupplyLiters,
        double soilDeficitAdjustedDemandLiters,
        double scheduledIrrigationLiters,
        double unmetDemandLiters,
        String status,
        double knowledgeFactor,
        double ecoImpactValue,
        double harmRisk
    ) {}

    private static double clamp01(double value) {
        return Math.max(0.0, Math.min(1.0, value));
    }

    private static Result schedule(
        double etoMm,
        double cropCoefficient,
        double irrigatedAreaM2,
        double soilMoistureDeficitFraction,
        double canalAllocationLiters,
        double conveyanceEfficiency,
        double habitatWaterReserveLiters
    ) {
        if (!Double.isFinite(etoMm) || !Double.isFinite(cropCoefficient) ||
            !Double.isFinite(irrigatedAreaM2) || !Double.isFinite(soilMoistureDeficitFraction) ||
            !Double.isFinite(canalAllocationLiters) || !Double.isFinite(conveyanceEfficiency) ||
            !Double.isFinite(habitatWaterReserveLiters) ||
            etoMm < 0.0 || cropCoefficient < 0.0 || irrigatedAreaM2 < 0.0 ||
            soilMoistureDeficitFraction < 0.0 || soilMoistureDeficitFraction > 1.0 ||
            canalAllocationLiters < 0.0 || conveyanceEfficiency < 0.0 || conveyanceEfficiency > 1.0 ||
            habitatWaterReserveLiters < 0.0) {
            throw new IllegalArgumentException("all input values must be finite and within declared bounds");
        }

        double etcMm = cropCoefficient * etoMm;
        double plantDemandLiters = etcMm * irrigatedAreaM2;
        double deficitAdjustedDemand = plantDemandLiters * soilMoistureDeficitFraction;
        double allocableCanalLiters = Math.max(0.0, canalAllocationLiters - habitatWaterReserveLiters);
        double usableCanalSupply = allocableCanalLiters * conveyanceEfficiency;
        double scheduled = Math.min(deficitAdjustedDemand, usableCanalSupply);
        double unmet = Math.max(0.0, deficitAdjustedDemand - scheduled);

        String status;
        if (scheduled <= 0.0 && deficitAdjustedDemand > 0.0) {
            status = "HOLD_INSUFFICIENT_ALLOCATED_WATER";
        } else if (unmet > 0.0) {
            status = "PARTIAL_IRRIGATION_WITH_UNMET_DEMAND";
        } else {
            status = "WITHIN_DECLARED_WATER_BUDGET";
        }

        double waterCoverage = deficitAdjustedDemand <= 0.0 ? 1.0 : scheduled / deficitAdjustedDemand;
        double knowledge = clamp01(0.65 + 0.20 * conveyanceEfficiency + 0.15 * (soilMoistureDeficitFraction > 0.0 ? 1.0 : 0.0));
        double impact = clamp01(0.75 * waterCoverage * (cropCoefficient > 0.0 ? 1.0 : 0.0));
        double risk = clamp01(0.15 + 0.70 * (1.0 - waterCoverage));

        return new Result(
            etcMm,
            plantDemandLiters,
            usableCanalSupply,
            deficitAdjustedDemand,
            scheduled,
            unmet,
            status,
            knowledge,
            impact,
            risk
        );
    }

    public static void main(String[] args) {
        if (args.length != 7) {
            System.err.println(
                "usage: EtIrrigationScheduler <ET0_mm> <Kc> <irrigated_area_m2> " +
                "<soil_moisture_deficit_fraction_0_to_1> <canal_allocation_L> " +
                "<conveyance_efficiency_0_to_1> <habitat_water_reserve_L>"
            );
            System.exit(64);
        }

        try {
            Result result = schedule(
                Double.parseDouble(args[0]),
                Double.parseDouble(args[1]),
                Double.parseDouble(args[2]),
                Double.parseDouble(args[3]),
                Double.parseDouble(args[4]),
                Double.parseDouble(args[5]),
                Double.parseDouble(args[6])
            );

            System.out.printf("ETc_mm=%.8f%n", result.etcMm());
            System.out.printf("plant_demand_L=%.8f%n", result.plantDemandLiters());
            System.out.printf("usable_canal_supply_L=%.8f%n", result.usableCanalSupplyLiters());
            System.out.printf("soil_deficit_adjusted_demand_L=%.8f%n", result.soilDeficitAdjustedDemandLiters());
            System.out.printf("scheduled_irrigation_L=%.8f%n", result.scheduledIrrigationLiters());
            System.out.printf("unmet_demand_L=%.8f%n", result.unmetDemandLiters());
            System.out.println("status=" + result.status());
            System.out.printf("knowledge_factor=%.8f%n", result.knowledgeFactor());
            System.out.printf("eco_impact_value=%.8f%n", result.ecoImpactValue());
            System.out.printf("harm_risk=%.8f%n", result.harmRisk());
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
