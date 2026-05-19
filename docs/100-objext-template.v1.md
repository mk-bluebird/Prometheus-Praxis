# 100-OBJEXT Session Template v1

session_id: <string>          # Stable ID for this 100-OBJEXT run
user_id:    <string>          # Optional, DID or internal ID
source:     <string>          # e.g., "web", "ios", "android", "terminal"
created_at: <RFC3339-timestamp>
input_summary: <short natural-language summary of the user's request>
profile: <string>             # research | exam | code-audit | governance | offline
shard_id: "OBJEXT-MAP-V1"

sections:
  - section_index: 1
    label: "section_1"
    objects:
      - obj_index: 1
        obj_id: "<session_id>-001"
        obj_type: "topic"        # topic|question|quiz|definition|task
        title: "<concise title>"
        prompt: "<full text of the topic/question/quiz item/etc.>"
        complexity_reason: "<why non-trivial, what real outcome it enables>"
        priority: 1              # 1–5, 1 = highest priority for this section
        tags: ["aln", "rust", "syntax-dev"]
      - obj_index: 2
        obj_id: "<session_id>-002"
        obj_type: "question"
        title: "<concise title>"
        prompt: "<full text>"
        complexity_reason: "<non-filler explanation>"
        priority: 2
        tags: []
      # ... objects 3–10

  - section_index: 2
    label: "section_2"
    objects:
      - obj_index: 11
        obj_id: "<session_id>-011"
        obj_type: "quiz"
        title: "<concise title>"
        prompt: "<question + answer schema>"
        complexity_reason: "<detail>"
        priority: 1
        tags: []
      # ... objects 12–20

  # ...
  # Repeat pattern up to section_index: 10 (objects 91–100)
  # ...

mapping_shard:
  filename: "schemas/objext/100-objext.mapping.v1.aln"
  shard_id: "OBJEXT-MAP-V1"
  description: "Bluetooth-style pattern mappings for 100-OBJEXT routing"

operational_rules:
  - "Exactly 100 objects per session unless a stricter upper bound is configured."
  - "Each object must be topic|question|quiz|definition|task."
  - "Every object includes a non-trivial complexity_reason tied to real outcomes."
  - "Sections are 10×10; no hypothetical or decorative filler."
  - "No schema rollback or reversal when evolving formats."
