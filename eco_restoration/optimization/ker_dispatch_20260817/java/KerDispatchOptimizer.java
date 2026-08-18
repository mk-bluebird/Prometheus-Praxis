import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public final class KerDispatchOptimizer {
    private KerDispatchOptimizer() {}

    private record Candidate(String id, double ecoImpactValue, double harmRisk, int energyReqJ) {}
    private record Plan(List<Candidate> selected, double impact, int energy, String status) {}

    private static Plan optimize(List<Candidate> candidates, double riskThreshold, int budget) {
        if (riskThreshold < 0.0 || riskThreshold > 1.0 || budget < 0 || budget > 200_000) {
            throw new IllegalArgumentException("invalid risk threshold or budget; exact edge solver supports budgets up to 200000 J");
        }

        List<Candidate> eligible = candidates.stream()
            .filter(candidate -> {
                if (candidate.ecoImpactValue() < 0.0 || candidate.ecoImpactValue() > 1.0 ||
                    candidate.harmRisk() < 0.0 || candidate.harmRisk() > 1.0 ||
                    candidate.energyReqJ() < 0) {
                    throw new IllegalArgumentException("candidate values outside KER dispatch bounds");
                }
                return candidate.harmRisk() <= riskThreshold && candidate.energyReqJ() <= budget;
            })
            .sorted(Comparator.comparing(Candidate::id))
            .toList();

        double[] value = new double[budget + 1];
        boolean[][] selected = new boolean[eligible.size()][budget + 1];

        for (int index = 0; index < eligible.size(); index++) {
            Candidate candidate = eligible.get(index);
            for (int energy = budget; energy >= candidate.energyReqJ(); energy--) {
                double include = value[energy - candidate.energyReqJ()] + candidate.ecoImpactValue();
                if (include > value[energy] + 1.0e-12) {
                    value[energy] = include;
                    selected[index][energy] = true;
                }
            }
        }

        int remaining = budget;
        List<Candidate> result = new ArrayList<>();
        for (int index = eligible.size() - 1; index >= 0; index--) {
            Candidate candidate = eligible.get(index);
            if (remaining >= candidate.energyReqJ() && selected[index][remaining]) {
                result.add(candidate);
                remaining -= candidate.energyReqJ();
            }
        }

        result.sort(Comparator.comparing(Candidate::id));
        int energy = result.stream().mapToInt(Candidate::energyReqJ).sum();
        double impact = result.stream().mapToDouble(Candidate::ecoImpactValue).sum();
        return new Plan(result, impact, energy, result.isEmpty() ? "NO_ELIGIBLE_DISPATCH" : "EXACT_KNAPSACK_PLAN");
    }

    public static void main(String[] args) {
        if (args.length < 6 || (args.length - 2) % 4 != 0) {
            System.err.println(
                "usage: KerDispatchOptimizer <risk_threshold> <energy_budget_J> " +
                "<id> <eco_impact> <harm_risk> <energyreqJ_integer> [...]"
            );
            System.exit(64);
        }

        try {
            double threshold = Double.parseDouble(args[0]);
            int budget = Integer.parseInt(args[1]);
            List<Candidate> candidates = new ArrayList<>();

            for (int index = 2; index < args.length; index += 4) {
                candidates.add(new Candidate(
                    args[index],
                    Double.parseDouble(args[index + 1]),
                    Double.parseDouble(args[index + 2]),
                    Integer.parseInt(args[index + 3])
                ));
            }

            Plan plan = optimize(candidates, threshold, budget);
            System.out.println("status=" + plan.status());
            System.out.printf("total_eco_impact_value=%.8f%n", plan.impact());
            System.out.println("total_energyreqJ=" + plan.energy());
            for (Candidate candidate : plan.selected()) {
                System.out.printf(
                    "selected id=%s eco_impact_value=%.8f harm_risk=%.8f energyreqJ=%d%n",
                    candidate.id(),
                    candidate.ecoImpactValue(),
                    candidate.harmRisk(),
                    candidate.energyReqJ()
                );
            }
        } catch (IllegalArgumentException exception) {
            System.err.println("error: " + exception.getMessage());
            System.exit(65);
        }
    }
}
