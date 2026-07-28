// filename: src/cpp/generated/cyboquatic_blastradius_governance_struct.hpp
// license: MIT OR Apache-2.0
// generated from ALN particle cyboquatic.blastradius.governance
#ifndef CYBOQUATIC_BLASTRADIUS_GOVERNANCE_STRUCT_HPP
#define CYBOQUATIC_BLASTRADIUS_GOVERNANCE_STRUCT_HPP

struct cyboquatic_blastradius_governance_struct {
    const char* blastIndexId;
    const char* nodeId;
    const char* eventId;
    const char* corridorId;
    const char* laneId;
    const char* alnShardName;
    const char* alnVersion;
    const char* didRoot;
    const char* eventTimestampUtc;
    const char* windowStartUtc;
    const char* windowEndUtc;
    double surchargeLevelM;
    double hydraulicHeadM;
    double inflowM3s;
    double durationS;
    double radiusM;
    double radiusNorm;
    double k;
    double e;
    double r;
    double kerScore;
    double residualKer;
    double rohCoordinate;
    bool radiusWithinLimit;
    bool kerWithinLimit;
    bool laneAdmissibleOk;
    bool safeToPromoteOk;
    const char* sourceDbPath;
    const char* sourceViewName;
    const char* evidenceHex;
    const char* createdAtUtc;
    const char* lastUpdatedUtc;
};

#endif // CYBOQUATIC_BLASTRADIUS_GOVERNANCE_STRUCT_HPP
