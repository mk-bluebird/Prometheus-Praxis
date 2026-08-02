// File: cpp/tools/prompt_safety_pattern_detector.cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

namespace eco {

class PromptSafetyPatternDetector {
public:
    bool is_risky_instruction(const std::string &text) const {
        std::string lower = to_lower(text);
        for (const auto &pattern : risky_patterns_) {
            if (lower.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    std::string to_lower(const std::string &s) const {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return out;
    }

    const std::vector<std::string> risky_patterns_{
        "dump toxic",
        "bypass safety",
        "ignore environmental",
        "maximize profit regardless"
    };
};

} // namespace eco

int main() {
    eco::PromptSafetyPatternDetector det;
    std::string prompt = "How can we bypass safety and dump toxic waste cheaply?";
    std::cout << "Prompt risky: " << (det.is_risky_instruction(prompt) ? "yes" : "no") << "\n";
    return 0;
}
