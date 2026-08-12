// File: cpp/eco_restoration/onnx_heat_risk_downscaler.cpp
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct HeatPredictors {
    float coarse_lst_c{};
    float ndvi{};
    float albedo{};
    float elevation_m{};
};

struct HeatRiskEstimate {
    double downscaled_lst_c{};
    double r_heat{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class OnnxHeatRiskDownscaler {
public:
    explicit OnnxHeatRiskDownscaler(const std::string& model_path)
        : environment_(ORT_LOGGING_LEVEL_WARNING, "eco_heat_downscaler"),
          session_(environment_, model_path.c_str(), session_options_) {
        session_options_.SetIntraOpNumThreads(1);
        Ort::AllocatorWithDefaultOptions allocator;
        input_name_ = session_.GetInputNameAllocated(0, allocator).get();
        output_name_ = session_.GetOutputNameAllocated(0, allocator).get();
    }

    HeatRiskEstimate predict(const HeatPredictors& predictors, double reference_c,
                             double range_c, double model_mae_c) const {
        if (range_c <= 0.0 || model_mae_c < 0.0) throw std::invalid_argument("invalid calibration");
        std::array<float, 4> values{predictors.coarse_lst_c, predictors.ndvi,
                                    predictors.albedo, predictors.elevation_m};
        std::array<int64_t, 2> shape{1, 4};
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto input = Ort::Value::CreateTensor<float>(
            memory, values.data(), values.size(), shape.data(), shape.size());
        const char* inputs[]{input_name_.c_str()};
        const char* outputs[]{output_name_.c_str()};
        auto result = session_.Run(Ort::RunOptions{nullptr}, inputs, &input, 1, outputs, 1);
        const float temperature = result.front().GetTensorMutableData<float>()[0];
        const double risk = std::clamp((temperature - reference_c) / range_c +
                                       model_mae_c / range_c, 0.0, 1.0);
        const double knowledge = std::clamp(1.0 - model_mae_c / range_c, 0.0, 1.0);
        return {temperature, risk, knowledge, knowledge * (1.0 - risk)};
    }

private:
    Ort::Env environment_;
    Ort::SessionOptions session_options_;
    Ort::Session session_;
    std::string input_name_, output_name_;
};

}  // namespace eco_restoration
