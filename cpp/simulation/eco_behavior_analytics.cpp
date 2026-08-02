// File: cpp/simulation/eco_behavior_analytics.cpp
#include <iostream>
#include <vector>
#include <string>

namespace eco {

struct BrowserAction {
    std::string url;
    bool eco_positive_content;
    double dwell_time_minutes;
};

class EcoBehaviorAnalytics {
public:
    double compute_eco_focus_score(const std::vector<BrowserAction> &actions) const {
        double eco_time = 0.0;
        double total_time = 0.0;
        for (const auto &a : actions) {
            total_time += a.dwell_time_minutes;
            if (a.eco_positive_content) eco_time += a.dwell_time_minutes;
        }
        if (total_time == 0.0) return 0.0;
        return eco_time / total_time;
    }
};

} // namespace eco

int main() {
    std::vector<eco::BrowserAction> actions{
        {"https://example.org/composting-guide", true, 10.0},
        {"https://example.org/general-news", false, 5.0},
        {"https://example.org/urban-greening", true, 7.0}
    };
    eco::EcoBehaviorAnalytics analytics;
    std::cout << "Eco focus score: " << analytics.compute_eco_focus_score(actions) << "\n";
    return 0;
}
