// src/browser/agents/ai-agent.js

import { createResponseStabilityDuty } from "../duties/ai-response-stability-duty.js";

/**
 * Example minimal model client; adapt to your real backend.
 */
const modelClient = {
  async generate({ prompt }) {
    const response = await fetch("/api/ai/generate", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({ prompt })
    });

    if (!response.ok) {
      throw new Error("Model backend error: " + response.status);
    }

    const data = await response.json();
    // Assume data.output is the raw text from the model.
    return String(data.output || "");
  }
};

const stabilityDuty = createResponseStabilityDuty(modelClient, {
  maxRetries: 4,
  minLength: 512,
  expectedSectionMarkers: ["## ", "### ", "- "]
});

export async function askModel(prompt) {
  const stableText = await stabilityDuty.generateStable({ prompt });
  return stableText;
}
