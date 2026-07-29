// src/browser/duties/ai-response-stability-duty.js

/**
 * ResponseCompletenessGuard
 *
 * A browser-duty module that wraps AI model calls to reduce "choking"
 * and incomplete responses. It applies heuristics to detect truncation
 * or low-coverage answers and auto-requests continuation/repair from
 * the model backend.
 *
 * This module is backend-agnostic: you provide a `modelClient` with
 * a simple `generate` method, and the guard handles retries/merging.
 */

/**
 * @typedef {Object} ModelGenerateOptions
 * @property {string} prompt - The main user/system prompt.
 * @property {string} [conversationId] - Optional conversation/thread id.
 * @property {Object} [meta] - Optional metadata for logging/tracking.
 */

/**
 * @typedef {Object} ModelClient
 * @property {(options: ModelGenerateOptions) => Promise<string>} generate
 *  The core method that takes a prompt and returns the model's text output.
 */

class ResponseCompletenessGuard {
  /**
   * @param {ModelClient} modelClient - Backend model client.
   * @param {Object} [config] - Guard configuration.
   * @param {number} [config.maxRetries=3] - Maximum repair/continuation attempts.
   * @param {number} [config.minLength=256] - Minimum acceptable character length.
   * @param {number} [config.sectionThreshold=0.6] - Fraction of expected sections required.
   * @param {string[]} [config.expectedSectionMarkers] - Markers like "##", "###", etc.
   */
  constructor(modelClient, config = {}) {
    this.modelClient = modelClient;
    this.maxRetries = typeof config.maxRetries === "number" ? config.maxRetries : 3;
    this.minLength = typeof config.minLength === "number" ? config.minLength : 256;
    this.sectionThreshold =
      typeof config.sectionThreshold === "number" ? config.sectionThreshold : 0.6;
    this.expectedSectionMarkers = Array.isArray(config.expectedSectionMarkers)
      ? config.expectedSectionMarkers
      : ["## ", "### "];
  }

  /**
   * High-level entry point: generate a stable, complete answer.
   *
   * @param {ModelGenerateOptions} options
   * @returns {Promise<string>} - A merged, completeness-checked response.
   */
  async generateStable(options) {
    const basePrompt = options.prompt;
    let attempts = 0;
    let accumulated = "";

    while (attempts <= this.maxRetries) {
      const currentPrompt =
        attempts === 0
          ? basePrompt
          : this._buildRepairPrompt(basePrompt, accumulated, attempts);

      const fragment = await this.modelClient.generate({
        ...options,
        prompt: currentPrompt
      });

      accumulated = this._mergeResponses(accumulated, fragment);

      if (this._isComplete(accumulated, basePrompt)) {
        return accumulated;
      }

      attempts += 1;
    }

    // Even if incomplete, return best-effort with an explicit note.
    return accumulated + "\n\n[Note: response may be partially incomplete after max retries.]";
  }

  /**
   * Build a repair/continuation prompt to coax the model into completing
   * missing sections or finishing interrupted thoughts.
   *
   * @param {string} basePrompt
   * @param {string} accumulated
   * @param {number} attempts
   * @returns {string}
   */
  _buildRepairPrompt(basePrompt, accumulated, attempts) {
    const safetyHeader =
      "You are a stability-focused assistant. Repair and complete the prior answer.\n" +
      "Do NOT repeat content that is already complete. Only:\n" +
      "- Finish incomplete sentences and paragraphs.\n" +
      "- Add missing sections or examples explicitly requested.\n" +
      "- Ensure the structure is coherent and self-contained.\n";

    const contextSummary =
      "Previous answer (may be truncated or incomplete):\n" +
      accumulated.slice(-4096); // recent tail, safe for token limits

    const instruction =
      "\n\nUser base prompt:\n" +
      basePrompt +
      "\n\nRepair attempt #" +
      attempts +
      ":\nProvide ONLY the missing or completing text, not a full repeat.";

    return safetyHeader + "\n\n" + contextSummary + instruction;
  }

  /**
   * Merge a newly generated fragment into the accumulated answer.
   * Simple concatenation with trimming to avoid runaway duplication.
   *
   * @param {string} accumulated
   * @param {string} fragment
   * @returns {string}
   */
  _mergeResponses(accumulated, fragment) {
    if (!accumulated) {
      return fragment.trim();
    }
    const trimmedFragment = fragment.trim();

    // Avoid naive duplication: if the fragment starts with the last sentence
    // of accumulated, strip that overlap.
    const lastSentences = this._extractTailSentences(accumulated, 2);
    let merged = accumulated;

    for (const sentence of lastSentences) {
      if (trimmedFragment.startsWith(sentence)) {
        const overlap = sentence.length;
        return merged + trimmedFragment.slice(overlap);
      }
    }

    return merged + "\n\n" + trimmedFragment;
  }

  /**
   * Heuristic completeness checks:
   * - Minimum length.
   * - Reasonable sentence termination (no abrupt mid-word cuts).
   * - Presence of expected section markers.
   *
   * @param {string} text
   * @param {string} basePrompt
   * @returns {boolean}
   */
  _isComplete(text, basePrompt) {
    const normalized = text.trim();

    if (normalized.length < this.minLength) {
      return false;
    }

    if (this._looksCutOff(normalized)) {
      return false;
    }

    if (!this._coversExpectedSections(normalized, basePrompt)) {
      return false;
    }

    return true;
  }

  /**
   * Detect if text seems cut off mid-sentence/mid-word.
   *
   * @param {string} text
   * @returns {boolean}
   */
  _looksCutOff(text) {
    const tail = text.slice(-200);
    const hardStopChars = [".", "!", "?", ":", ";", "]", "}", "\"", "'"];

    const lastNonSpaceChar = tail.trim().slice(-1);

    if (!lastNonSpaceChar) {
      return true;
    }

    // If the last character is not a typical sentence or section terminator,
    // it may be cut off.
    if (!hardStopChars.includes(lastNonSpaceChar)) {
      return true;
    }

    // Also check if we end mid-token, e.g., "incomple".
    const words = tail.trim().split(/\s+/);
    const lastWord = words[words.length - 1] || "";
    const suspiciousEndings = ["ing", "ed", "ion", "ive", "al", "ent"];

    if (lastWord.length > 6 && !suspiciousEndings.some((s) => lastWord.endsWith(s))) {
      // This is a weak heuristic; we only flag obvious cut-offs.
      return false;
    }

    return false;
  }

  /**
   * Check if the text seems to cover expected sections implied by the base prompt.
   *
   * @param {string} text
   * @param {string} basePrompt
   * @returns {boolean}
   */
  _coversExpectedSections(text, basePrompt) {
    const expectedCount = this._inferExpectedSections(basePrompt);
    if (expectedCount === 0) {
      return true;
    }

    const observedMarkers = this.expectedSectionMarkers.reduce((count, marker) => {
      const matches = text.split(marker).length - 1;
      return count + matches;
    }, 0);

    const ratio = observedMarkers / expectedCount;
    return ratio >= this.sectionThreshold;
  }

  /**
   * Infer a rough number of sections from the base prompt:
   * count occurrences of "step", "list", "compare", etc., plus explicit numbering hints.
   *
   * @param {string} basePrompt
   * @returns {number}
   */
  _inferExpectedSections(basePrompt) {
    const lower = basePrompt.toLowerCase();
    let sections = 0;

    const hints = ["step", "steps", "list", "bullet", "compare", "sections", "parts"];
    for (const hint of hints) {
      if (lower.includes(hint)) {
        sections += 1;
      }
    }

    const explicitNumbers = (lower.match(/\b(\d+)\b/g) || []).map((n) => parseInt(n, 10));
    const maxExplicit = explicitNumbers.length ? Math.max(...explicitNumbers) : 0;

    if (maxExplicit > sections) {
      sections = maxExplicit;
    }

    return sections;
  }

  /**
   * Extract the last N sentences from text.
   *
   * @param {string} text
   * @param {number} n
   * @returns {string[]}
   */
  _extractTailSentences(text, n) {
    const normalized = text.replace(/\s+/g, " ").trim();
    const sentences = normalized.split(/(?<=[.!?])\s+/);
    const tail = sentences.slice(-n);
    return tail;
  }
}

/**
 * Factory function to wire the guard around any model client.
 *
 * @param {ModelClient} modelClient
 * @param {Object} [config]
 * @returns {{ generateStable: (options: ModelGenerateOptions) => Promise<string> }}
 */
function createResponseStabilityDuty(modelClient, config = {}) {
  const guard = new ResponseCompletenessGuard(modelClient, config);
  return {
    generateStable: (options) => guard.generateStable(options)
  };
}

export { ResponseCompletenessGuard, createResponseStabilityDuty };
