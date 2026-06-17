// include/eco_llm_client.hpp
#pragma once
#include <string>
#include <vector>

namespace eco {

struct ChatMessage {
    std::string role;   // "system", "user", "assistant"
    std::string content;
};

struct ChatResponse {
    std::string content;
};

class LlmClient {
public:
    virtual ~LlmClient() = default;
    virtual ChatResponse chat_completion(
        const std::vector<ChatMessage>& messages
    ) const = 0;
};

} // namespace eco
