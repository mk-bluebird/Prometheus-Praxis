// File: java/src/main/java/org/cyboquatic/workload/EcoAuditBridge.java
package org.cyboquatic.workload;

import org.cyboquatic.audit.EcoAuditService;

/**
 * Java-Kotlin bridge for eco-audit logging.
 */
public class EcoAuditBridge {

    public static void logKerEViolation(String did,
                                        String basinId,
                                        double timestampS,
                                        double kerE,
                                        String message) {
        EcoAuditService.logKerEViolation(did, basinId, timestampS, kerE, message);
    }
}
