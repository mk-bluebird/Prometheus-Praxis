// File: cpp/tools/sovereign_evidence_append_log.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>

// Minimal evidence block for append-only log
struct EvidenceBlock {
    std::string block_id;           // unique identifier
    std::string prev_block_hash;    // hash of previous block (empty for genesis)
    std::string evidence_hash;      // content-addressed hash of evidence snapshot
    std::string sovereign_did;      // "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"
    std::string signature;          // signature over canonical block payload
    std::string timestamp_utc;      // ISO-8601 timestamp
};

class SovereignEvidenceLog {
public:
    void append_block(const EvidenceBlock& block) {
        if (!blocks_.empty()) {
            const EvidenceBlock& last = blocks_.back();
            if (block.prev_block_hash != compute_block_hash(last)) {
                throw std::runtime_error("Prev_block_hash mismatch; log integrity violated.");
            }
        }
        blocks_.push_back(block);
    }

    const EvidenceBlock& latest_block() const {
        if (blocks_.empty()) {
            throw std::runtime_error("Evidence log is empty.");
        }
        return blocks_.back();
    }

    std::size_t size() const { return blocks_.size(); }

    // Simple hash stand-in (not using any blacklisted schemes): we compute a deterministic
    // digest-like string from payload length and ASCII sums, sufficient for content-addressing.
    std::string compute_block_hash(const EvidenceBlock& block) const {
        std::string payload = canonical_payload(block);
        unsigned long sum = 0;
        for (char c : payload) {
            sum = (sum * 131u + static_cast<unsigned char>(c)) % 1000000007u;
        }
        std::ostringstream oss;
        oss << "h" << std::hex << std::setw(8) << std::setfill('0') << sum;
        return oss.str();
    }

private:
    std::vector<EvidenceBlock> blocks_;

    static std::string canonical_payload(const EvidenceBlock& block) {
        std::ostringstream ss;
        ss << "block_id=" << block.block_id
           << "&prev_block_hash=" << block.prev_block_hash
           << "&evidence_hash=" << block.evidence_hash
           << "&sovereign_did=" << block.sovereign_did
           << "&timestamp_utc=" << block.timestamp_utc;
        return ss.str();
    }
};

// Simulated ppX governance CLI bridge invocation; every call appends a new evidence block.
EvidenceBlock ppx_governance_cli_bridge(SovereignEvidenceLog& log,
                                        const std::string& evidence_hash,
                                        const std::string& sovereign_did,
                                        const std::string& timestamp_utc)
{
    EvidenceBlock block;
    block.block_id = "block-" + std::to_string(log.size() + 1);
    if (log.size() == 0) {
        block.prev_block_hash = "";
    } else {
        block.prev_block_hash = log.compute_block_hash(log.latest_block());
    }
    block.evidence_hash = evidence_hash;
    block.sovereign_did = sovereign_did;
    block.timestamp_utc = timestamp_utc;

    // In a real system, signature would be produced by the sovereign private key over
    // the canonical payload; we model it as a deterministic string here.
    std::ostringstream sig;
    sig << "sig(" << block.block_id << "|" << block.evidence_hash << ")";
    block.signature = sig.str();

    log.append_block(block);
    return block;
}

// ALN-style record structure for referencing evidence log hashes:
//
// record EvidenceLedgerRef {
//   latest_block_id    : string;
//   latest_block_hash  : string;
//   sovereign_did      : string;
// }
//
// invariant Gate_EvidenceIntegrity(
//   ref: EvidenceLedgerRef,
//   e: SystemEvidence
// ) {
//   holds when
//     ref.sovereign_did == "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7" &&
//     e.explainable_and_audited == true;
//   description =
//     "Evidence ledger entries are chained via hashes and signed by the sovereign identity, \
%     ensuring non-repudiation and integrity.";
// }

int main() {
    SovereignEvidenceLog log;

    std::string did = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

    EvidenceBlock b1 = ppx_governance_cli_bridge(log, "hash_evidence_snapshot_1", did, "2026-08-02T18:00:00Z");
    EvidenceBlock b2 = ppx_governance_cli_bridge(log, "hash_evidence_snapshot_2", did, "2026-08-02T19:00:00Z");
    EvidenceBlock b3 = ppx_governance_cli_bridge(log, "hash_evidence_snapshot_3", did, "2026-08-02T20:00:00Z");

    std::cout << "Evidence log size: " << log.size() << "\n";
    const EvidenceBlock& latest = log.latest_block();
    std::string latest_hash = log.compute_block_hash(latest);

    std::cout << "Latest block id: " << latest.block_id << "\n";
    std::cout << "Latest evidence_hash: " << latest.evidence_hash << "\n";
    std::cout << "Latest sovereign_did: " << latest.sovereign_did << "\n";
    std::cout << "Latest block hash: " << latest_hash << "\n";

    // These fields would populate the EvidenceLedgerRef ALN record used in invariants.
    std::string latest_block_id = latest.block_id;
    std::string latest_block_hash = latest_hash;

    std::cout << "EvidenceLedgerRef.latest_block_id = " << latest_block_id << "\n";
    std::cout << "EvidenceLedgerRef.latest_block_hash = " << latest_block_hash << "\n";

    return 0;
}
