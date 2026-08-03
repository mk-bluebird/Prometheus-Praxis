// File: java/src/main/java/eco/testbed/CyberPhysicalTestbedCoordinator.java
package eco.testbed;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.sql.*;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;

/**
 * Java-Based Cyber-Physical Testbed Coordinator
 *
 * Wraps the functionality of the existing phoenix_testbed_planner.cpp (conceptually)
 * to schedule real hardware tests and feed results back into governance.
 *
 * Responsibilities:
 *  - Read planned tests from `phoenix_testbed_plan`.
 *  - Invoke the C++ planner or executor (via stdio) for each test.
 *  - Record test execution results into `phoenix_testbed_results`.
 *  - Ensure Lyapunov/carbon corridor constraints are respected by consulting
 *    hex-level stability/corridor tables before executing tests.
 */
public class CyberPhysicalTestbedCoordinator {

    public static class TestPlan {
        public String planId;
        public String testId;
        public String hexId;
        public String hardwareId;
        public String scenario;
    }

    public static class TestResult {
        public String planId;
        public String testId;
        public String hexId;
        public String hardwareId;
        public String scenario;
        public boolean executed;
        public boolean corridorSafe;
        public String detail;
    }

    private final String sqlitePath;
    private final String cppPlannerPath;

    public CyberPhysicalTestbedCoordinator(String sqlitePath, String cppPlannerPath) {
        this.sqlitePath = sqlitePath;
        this.cppPlannerPath = cppPlannerPath;
    }

    private Connection connect() throws SQLException {
        return DriverManager.getConnection("jdbc:sqlite:" + sqlitePath);
    }

    private List<TestPlan> loadPendingPlans(Connection conn) throws SQLException {
        List<TestPlan> plans = new ArrayList<>();
        String sql = "SELECT plan_id, test_id, hex_id, hardware_id, scenario " +
                     "FROM phoenix_testbed_plan WHERE status = 'PENDING'";
        try (PreparedStatement ps = conn.prepareStatement(sql);
             ResultSet rs = ps.executeQuery()) {
            while (rs.next()) {
                TestPlan p = new TestPlan();
                p.planId = rs.getString("plan_id");
                p.testId = rs.getString("test_id");
                p.hexId = rs.getString("hex_id");
                p.hardwareId = rs.getString("hardware_id");
                p.scenario = rs.getString("scenario");
                plans.add(p);
            }
        }
        return plans;
    }

    // Check Lyapunov/carbon corridor safety for the hex before executing a test.
    private boolean hexCorridorSafe(Connection conn, String hexId) throws SQLException {
        String sql = "SELECT v_residual, carbon_intensity, max_carbon " +
                     "FROM hex_stability_carbon WHERE hex_id = ?";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, hexId);
            try (ResultSet rs = ps.executeQuery()) {
                if (!rs.next()) {
                    return false;
                }
                double vResidual = rs.getDouble("v_residual");
                double ci = rs.getDouble("carbon_intensity");
                double maxCarbon = rs.getDouble("max_carbon");
                double c = 1.0 - ci / maxCarbon;
                return vResidual <= 1.0 && c >= 0.2;
            }
        }
    }

    // Invoke the C++ testbed planner/executor for a given test scenario.
    private String invokeCppPlanner(TestPlan plan) throws IOException, InterruptedException {
        Process proc = new ProcessBuilder(cppPlannerPath)
                .redirectErrorStream(true)
                .start();

        try (OutputStreamWriter writer = new OutputStreamWriter(proc.getOutputStream())) {
            // Simple protocol: send scenario line, e.g. "hexId hardwareId scenario"
            writer.write(plan.hexId + " " + plan.hardwareId + " " + plan.scenario);
            writer.write("\n");
            writer.flush();
        }

        StringBuilder sb = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(proc.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
        }
        proc.waitFor();
        return sb.toString();
    }

    private void updatePlanStatus(Connection conn, String planId, String status) throws SQLException {
        String sql = "UPDATE phoenix_testbed_plan SET status = ? WHERE plan_id = ?";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, status);
            ps.setString(2, planId);
            ps.executeUpdate();
        }
    }

    private void writeResult(Connection conn, TestResult result) throws SQLException {
        String sql = "INSERT INTO phoenix_testbed_results " +
                     "(plan_id, test_id, hex_id, hardware_id, scenario, executed, corridor_safe, detail, ts) " +
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            ps.setString(1, result.planId);
            ps.setString(2, result.testId);
            ps.setString(3, result.hexId);
            ps.setString(4, result.hardwareId);
            ps.setString(5, result.scenario);
            ps.setBoolean(6, result.executed);
            ps.setBoolean(7, result.corridorSafe);
            ps.setString(8, result.detail);
            ps.setString(9, LocalDateTime.now().format(DateTimeFormatter.ISO_LOCAL_DATE_TIME));
            ps.executeUpdate();
        }
    }

    public void runOnce() throws SQLException, IOException, InterruptedException {
        try (Connection conn = connect()) {
            conn.setAutoCommit(false);
            List<TestPlan> plans = loadPendingPlans(conn);
            for (TestPlan p : plans) {
                boolean safe = hexCorridorSafe(conn, p.hexId);
                TestResult r = new TestResult();
                r.planId = p.planId;
                r.testId = p.testId;
                r.hexId = p.hexId;
                r.hardwareId = p.hardwareId;
                r.scenario = p.scenario;
                r.corridorSafe = safe;

                if (!safe) {
                    r.executed = false;
                    r.detail = "Skipped: hex corridor not safe for test.";
                    updatePlanStatus(conn, p.planId, "SKIPPED");
                } else {
                    String cppOutput = invokeCppPlanner(p);
                    r.executed = true;
                    r.detail = cppOutput.trim();
                    updatePlanStatus(conn, p.planId, "COMPLETED");
                }

                writeResult(conn, r);
            }
            conn.commit();
        }
    }

    public static void main(String[] args) throws Exception {
        String sqlitePath = "prometheus_praxis.db";
        String cppPlannerPath = "./phoenix_testbed_planner"; // existing C++ binary
        if (args.length >= 1) sqlitePath = args[0];
        if (args.length >= 2) cppPlannerPath = args[1];

        CyberPhysicalTestbedCoordinator coord =
                new CyberPhysicalTestbedCoordinator(sqlitePath, cppPlannerPath);
        coord.runOnce();
        System.out.println("Cyber-physical testbed coordination run completed.");
    }
}
