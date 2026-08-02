// File: cpp/middleware/edge_model_runner.cpp
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>

// edge_model_runner:
// - Simulates loading an ONNX model for tree canopy detection.
// - Runs inference on a synthetic camera frame stream.
// - Pushes detection results to a ZeroMQ-like socket consumed by an eco-CLI dashboard.
//
// To avoid external dependencies, this file does not use real ONNX or ZeroMQ APIs;
// instead, it mimics their behavior with simple C++ classes.

namespace eco {

// Dummy ONNX model runner: in real deployment, use ONNX Runtime C++ API.
class OnnxCanopyModel {
public:
    explicit OnnxCanopyModel(const std::string& model_path)
        : model_path_(model_path), rng_(1234) {
        std::cout << "[OnnxCanopyModel] Loaded model from " << model_path_ << "\n";
    }

    // Run inference on a "frame"; here we just return a canopy fraction.
    double infer_canopy_fraction(const std::vector<uint8_t>& frame_bytes) {
        // Simulate canopy detection as a random fraction with mild smoothing.
        std::uniform_real_distribution<double> dist(0.2, 0.9);
        double raw = dist(rng_);
        // Adjust by frame size to induce minor variation.
        double scale = std::min(1.0, frame_bytes.size() / 10000.0);
        double canopy = raw * (0.5 + 0.5 * scale);
        return canopy;
    }

private:
    std::string model_path_;
    std::mt19937 rng_;
};

// Dummy ZeroMQ publisher: prints JSON-like messages.
class ZmqPublisher {
public:
    explicit ZmqPublisher(const std::string& endpoint)
        : endpoint_(endpoint) {
        std::cout << "[ZmqPublisher] Bound to " << endpoint_ << "\n";
    }

    void send(const std::string& topic, const std::string& payload) {
        std::cout << "[ZMQ] Topic: " << topic << "\n";
        std::cout << "      Payload: " << payload << "\n";
    }

private:
    std::string endpoint_;
};

struct CameraFrame {
    int width;
    int height;
    std::vector<uint8_t> data; // grayscale pixels
};

class CameraStreamSimulator {
public:
    CameraStreamSimulator(int width, int height)
        : width_(width), height_(height), rng_(42) {}

    CameraFrame next_frame() {
        CameraFrame f;
        f.width = width_;
        f.height = height_;
        f.data.resize(width_ * height_);

        std::uniform_int_distribution<int> pix(0, 255);
        for (auto& b : f.data) {
            b = static_cast<uint8_t>(pix(rng_));
        }
        return f;
    }

private:
    int width_;
    int height_;
    std::mt19937 rng_;
};

class EdgeModelRunner {
public:
    EdgeModelRunner(const std::string& model_path,
                    const std::string& zmq_endpoint)
        : model_(model_path),
          zmq_(zmq_endpoint),
          camera_(640, 480) {}

    void run_loop(int frames, double interval_seconds) {
        std::cout << "Starting edge model runner loop.\n";
        for (int i = 0; i < frames; ++i) {
            auto frame = camera_.next_frame();
            double canopy_fraction = model_.infer_canopy_fraction(frame.data);

            std::string payload = build_payload(canopy_fraction);
            zmq_.send("eco/canopy_detection", payload);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(interval_seconds * 1000))
            );
        }
        std::cout << "Edge model runner loop complete.\n";
    }

private:
    OnnxCanopyModel model_;
    ZmqPublisher zmq_;
    CameraStreamSimulator camera_;

    static std::string build_payload(double canopy_fraction) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "{"
            << "\"canopy_fraction\":" << canopy_fraction
            << "}";
        return oss.str();
    }
};

} // namespace eco

int main() {
    using namespace eco;

    EdgeModelRunner runner(
        "models/tree_canopy_detection.onnx",
        "tcp://0.0.0.0:5555"
    );

    runner.run_loop(5, 1.0);

    return 0;
}
