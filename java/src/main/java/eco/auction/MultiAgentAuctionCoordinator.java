// File: java/src/main/java/eco/auction/MultiAgentAuctionCoordinator.java
package eco.auction;

import java.sql.*;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

// Java shard implementing the sealed-bid auction clearing mechanism,
// wired to Prometheus-Praxis via JDBC and aligned with the C++ auction semantics
// documented in java_multi_agent_auction_reference.cpp.

public class MultiAgentAuctionCoordinator {

    public static class AuctionBid {
        public String auctionId;
        public String agentId;
        public String hexId;
        public double bidKerS;
        public double bidCarbonReduction;
        public double bidCost;
    }

    public static class AuctionAllocation {
        public String auctionId;
        public String agentId;
        public String hexId;
        public boolean allocated;
        public double clearingKerS;
        public double clearingCost;
    }

    // Corridor filter: read hex-level Lyapunov/carbon context and discard unsafe bids.
    private boolean bidRespectsCorridor(Connection conn, AuctionBid bid) throws SQLException {
        try (PreparedStatement ps = conn.prepareStatement(
                "SELECT v_residual, carbon_intensity, max_carbon, ker_s_min_prod " +
                "FROM hex_stability_carbon WHERE hex_id = ?")) {
            ps.setString(1, bid.hexId);
            try (ResultSet rs = ps.executeQuery()) {
                if (!rs.next()) return false;
                double vResidual = rs.getDouble("v_residual");
                double ci = rs.getDouble("carbon_intensity");
                double maxCarbon = rs.getDouble("max_carbon");
                double kerSMinProd = rs.getDouble("ker_s_min_prod");

                double corridorCarbon = 1.0 - ci / maxCarbon;
                if (corridorCarbon < 0.2) {
                    // Too carbon-stressed to accept new capacity.
                    return false;
                }
                if (vResidual > 1.0) {
                    // Lyapunov residual too high.
                    return false;
                }
                if (bid.bidKerS < kerSMinProd) {
                    // Bidder does not meet minimum KER for PROD-like capacity.
                    return false;
                }
                return true;
            }
        }
    }

    private List<AuctionBid> loadBids(Connection conn, String auctionId) throws SQLException {
        List<AuctionBid> bids = new ArrayList<>();
        try (PreparedStatement ps = conn.prepareStatement(
                "SELECT auction_id, agent_id, hex_id, bid_ker_s, bid_carbon_reduction, bid_cost " +
                "FROM auction_bids WHERE auction_id = ?")) {
            ps.setString(1, auctionId);
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    AuctionBid b = new AuctionBid();
                    b.auctionId = rs.getString("auction_id");
                    b.agentId = rs.getString("agent_id");
                    b.hexId = rs.getString("hex_id");
                    b.bidKerS = rs.getDouble("bid_ker_s");
                    b.bidCarbonReduction = rs.getDouble("bid_carbon_reduction");
                    b.bidCost = rs.getDouble("bid_cost");
                    bids.add(b);
                }
            }
        }
        return bids;
    }

    // Select winning bids by maximizing ker_s per unit cost, under a simple budget cap.
    private List<AuctionAllocation> selectAllocations(Connection conn,
                                                      List<AuctionBid> safeBids,
                                                      String auctionId) throws SQLException {
        double budgetCap;
        try (PreparedStatement ps = conn.prepareStatement(
                "SELECT budget_cap FROM auction_config WHERE auction_id = ?")) {
            ps.setString(1, auctionId);
            try (ResultSet rs = ps.executeQuery()) {
                budgetCap = rs.next() ? rs.getDouble("budget_cap") : Double.POSITIVE_INFINITY;
            }
        }

        safeBids.sort(Comparator.comparingDouble(b -> -(b.bidKerS / Math.max(b.bidCost, 1e-6))));

        double spent = 0.0;
        List<AuctionAllocation> allocs = new ArrayList<>();
        for (AuctionBid b : safeBids) {
            boolean allocate = (spent + b.bidCost <= budgetCap);
            AuctionAllocation a = new AuctionAllocation();
            a.auctionId = b.auctionId;
            a.agentId = b.agentId;
            a.hexId = b.hexId;
            a.allocated = allocate;
            a.clearingKerS = allocate ? b.bidKerS : 0.0;
            a.clearingCost = allocate ? b.bidCost : 0.0;

            if (allocate) {
                spent += b.bidCost;
            }
            allocs.add(a);
        }
        return allocs;
    }

    private void writeAllocations(Connection conn, List<AuctionAllocation> allocs) throws SQLException {
        try (PreparedStatement delete = conn.prepareStatement(
                "DELETE FROM auction_allocations WHERE auction_id = ?")) {
            if (!allocs.isEmpty()) {
                delete.setString(1, allocs.get(0).auctionId);
                delete.executeUpdate();
            }
        }

        try (PreparedStatement ps = conn.prepareStatement(
                "INSERT INTO auction_allocations " +
                "(auction_id, agent_id, hex_id, allocated, clearing_ker_s, clearing_cost) " +
                "VALUES (?, ?, ?, ?, ?, ?)")) {
            for (AuctionAllocation a : allocs) {
                ps.setString(1, a.auctionId);
                ps.setString(2, a.agentId);
                ps.setString(3, a.hexId);
                ps.setBoolean(4, a.allocated);
                ps.setDouble(5, a.clearingKerS);
                ps.setDouble(6, a.clearingCost);
                ps.addBatch();
            }
            ps.executeBatch();
        }
    }

    public void clearAuction(Connection conn, String auctionId) throws SQLException {
        List<AuctionBid> bids = loadBids(conn, auctionId);
        List<AuctionBid> safe = new ArrayList<>();
        for (AuctionBid b : bids) {
            if (bidRespectsCorridor(conn, b)) {
                safe.add(b);
            }
        }
        List<AuctionAllocation> allocs = selectAllocations(conn, safe, auctionId);
        writeAllocations(conn, allocs);
    }

    // Example main demonstrating usage; in production, this would be called from CI or a scheduler.
    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.err.println("Usage: MultiAgentAuctionCoordinator <auction_id>");
            return;
        }
        String auctionId = args[0];

        // Example JDBC connection; adjust URL/credentials for Prometheus-Praxis.
        String url = "jdbc:sqlite:prometheus_praxis.db";
        try (Connection conn = DriverManager.getConnection(url)) {
            conn.setAutoCommit(false);
            MultiAgentAuctionCoordinator coord = new MultiAgentAuctionCoordinator();
            coord.clearAuction(conn, auctionId);
            conn.commit();
            System.out.println("Auction " + auctionId + " cleared under Lyapunov/carbon constraints.");
        }
    }
}
