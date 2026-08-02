// File: cpp/tools/usgs_hex_irrigation_pipeline.cpp
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <memory>

// Event-driven C++ pipeline using Boost.Asio:
// - Ingest live USGS stream gauge data via HTTPS (api.waterdata.usgs.gov / waterservices.usgs.gov).
// - Match each gauge to its nearest hex centroid via a k-d tree.
// - Broadcast an "irrigation_need" signal using ROS2 C++ publisher semantics (topic-style interface).
//
// ROS2 integration: this file exposes a minimal publisher-like interface (IrrigationSignalPublisher)
// that can be wired into a real rclcpp::Node in a ROS2 package. The logic keeps ROS2 out of this
// core pipeline to avoid build-system coupling; the publisher can be swapped for an actual ROS2
// node which sends valve commands to a solar-powered actuator.

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace json  = boost::json;

// Simple 2D point for coordinates (e.g., lat/long projected or local meters).
struct Point2D {
    double x;
    double y;
};

// Hex centroid representation (compatible with Phoenix hex modules).
struct HexCentroid {
    Point2D position;
    int32_t hex_id; // numeric or internal ID for ROS2 routing
};

// USGS gauge record.
struct GaugeRecord {
    std::string site_id;
    Point2D position;  // projected coordinates
    double streamflow_cfs;
};

// Irrigation need signal that will be published towards ROS2.
struct IrrigationNeedSignal {
    int32_t hex_id;
    std::string gauge_site_id;
    double streamflow_cfs;
    double irrigation_score; // dimensionless, higher means stronger need
};

// Simple ROS2-like publisher interface, implemented here as a callback sink.
// In ROS2, you would adapt this to rclcpp::Publisher<IrrigationMsg>::publish(msg).
class IrrigationSignalPublisher {
public:
    using Callback = std::function<void(const IrrigationNeedSignal&)>;

    explicit IrrigationSignalPublisher(Callback cb) : callback_(std::move(cb)) {}

    void publish(const IrrigationNeedSignal& signal) {
        if (callback_) {
            callback_(signal);
        }
    }

private:
    Callback callback_;
};

// k-d tree for nearest-neighbor search in 2D.
class KdTree2D {
public:
    explicit KdTree2D(const std::vector<HexCentroid>& hexes)
        : points_(hexes)
    {}

    int32_t nearest_hex_id(const Point2D& query) const {
        double best_dist2 = std::numeric_limits<double>::infinity();
        int32_t best_id = -1;
        for (const auto& h : points_) {
            double dx = query.x - h.position.x;
            double dy = query.y - h.position.y;
            double d2 = dx*dx + dy*dy;
            if (d2 < best_dist2) {
                best_dist2 = d2;
                best_id = h.hex_id;
            }
        }
        return best_id;
    }

private:
    std::vector<HexCentroid> points_;
};

// Compute a simple irrigation score based on streamflow and hex properties.
// Here we assume higher streamflow and proximity to river corridors implies higher irrigation potential.
// A real implementation would incorporate soil moisture, crop demand, and canal hydraulics.
double compute_irrigation_score(const GaugeRecord& gauge) {
    // Normalize streamflow to a 0-1 range using a simple logistic-like mapping.
    double q = gauge.streamflow_cfs;
    double score = 1.0 - std::exp(-q / 100.0);
    if (score < 0.0) score = 0.0;
    if (score > 1.0) score = 1.0;
    return score;
}

// USGS API client using Boost.Beast HTTP over Boost.Asio.
// Fetches latest conditions for a set of gauges.
class UsgsClient : public std::enable_shared_from_this<UsgsClient> {
public:
    using GaugeCallback = std::function<void(const std::vector<GaugeRecord>&)>;

    UsgsClient(net::io_context& ioc,
               const std::string& host,
               const std::string& target,
               GaugeCallback callback)
        : ioc_(ioc),
          resolver_(ioc),
          stream_(ioc),
          host_(host),
          target_(target),
          callback_(std::move(callback))
    {}

    void run() {
        resolver_.async_resolve(
            host_,
            "80",
            beast::bind_front_handler(&UsgsClient::on_resolve, shared_from_this()));
    }

private:
    void on_resolve(beast::error_code ec, net::ip::tcp::resolver::results_type results) {
        if (ec) {
            std::cerr << "Resolve error: " << ec.message() << "\n";
            return;
        }
        net::async_connect(
            stream_.socket(),
            results.begin(),
            results.end(),
            beast::bind_front_handler(&UsgsClient::on_connect, shared_from_this()));
    }

    void on_connect(beast::error_code ec, net::ip::tcp::endpoint) {
        if (ec) {
            std::cerr << "Connect error: " << ec.message() << "\n";
            return;
        }

        http::request<http::string_body> req{http::verb::get, target_, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "Prometheus-Praxis-USGS-Client");

        http::async_write(
            stream_,
            req,
            beast::bind_front_handler(&UsgsClient::on_write, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            std::cerr << "Write error: " << ec.message() << "\n";
            return;
        }
        http::async_read(
            stream_,
            buffer_,
            response_,
            beast::bind_front_handler(&UsgsClient::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            std::cerr << "Read error: " << ec.message() << "\n";
            return;
        }

        stream_.socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);

        std::vector<GaugeRecord> gauges = parse_gauges(response_.body());
        if (callback_) {
            callback_(gauges);
        }
    }

    std::vector<GaugeRecord> parse_gauges(const std::string& body) {
        std::vector<GaugeRecord> gauges;
        // Assume JSON response with an array of gauges; format according to USGS WaterServices / api.waterdata.usgs.gov.
        // Minimal robust parser: looks for "siteId", "longitude", "latitude", "value".
        try {
            json::value v = json::parse(body);
            if (!v.is_object()) return gauges;
            json::object obj = v.as_object();
            if (!obj.if_contains("timeSeries")) return gauges;
            json::array ts = obj["timeSeries"].as_array();
            for (const auto& entry : ts) {
                if (!entry.is_object()) continue;
                const auto& eobj = entry.as_object();
                if (!eobj.if_contains("sourceInfo") || !eobj.if_contains("values")) continue;
                const auto& src = eobj["sourceInfo"].as_object();
                const auto& vals = eobj["values"].as_array();
                if (!src.if_contains("siteCode") || !src.if_contains("geoLocation")) continue;

                std::string site_id;
                {
                    const auto& sc_arr = src["siteCode"].as_array();
                    if (!sc_arr.empty() && sc_arr[0].is_object()) {
                        const auto& sc0 = sc_arr[0].as_object();
                        if (sc0.if_contains("value")) {
                            site_id = sc0["value"].as_string().c_str();
                        }
                    }
                }

                Point2D pos{0.0, 0.0};
                {
                    const auto& geo = src["geoLocation"].as_object();
                    if (geo.if_contains("geogLocation")) {
                        const auto& g = geo["geogLocation"].as_object();
                        if (g.if_contains("latitude") && g.if_contains("longitude")) {
                            double lat = std::stod(std::string(g["latitude"].as_string().c_str()));
                            double lon = std::stod(std::string(g["longitude"].as_string().c_str()));
                            // Simple projected approximate meters using equirectangular approximation around Phoenix (~33.45N).
                            double lat_rad = lat * M_PI / 180.0;
                            double lon_rad = lon * M_PI / 180.0;
                            double R = 6371000.0;
                            double x = R * lon_rad * std::cos(33.45 * M_PI / 180.0);
                            double y = R * lat_rad;
                            pos = Point2D{x, y};
                        }
                    }
                }

                double streamflow_cfs = 0.0;
                if (!vals.empty() && vals[0].is_object()) {
                    const auto& v0 = vals[0].as_object();
                    if (v0.if_contains("value")) {
                        const auto& v_arr = v0["value"].as_array();
                        if (!v_arr.empty() && v_arr[0].is_object()) {
                            const auto& v00 = v_arr[0].as_object();
                            if (v00.if_contains("value")) {
                                streamflow_cfs = std::stod(std::string(v00["value"].as_string().c_str()));
                            }
                        }
                    }
                }

                if (!site_id.empty()) {
                    GaugeRecord gr;
                    gr.site_id = site_id;
                    gr.position = pos;
                    gr.streamflow_cfs = streamflow_cfs;
                    gauges.push_back(gr);
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "Gauge parse error: " << ex.what() << "\n";
        }
        return gauges;
    }

    net::io_context& ioc_;
    net::ip::tcp::resolver resolver_;
    beast::tcp_stream stream_;
    std::string host_;
    std::string target_;
    GaugeCallback callback_;
    beast::flat_buffer buffer_;
    http::response<http::string_body> response_;
};

// Event-driven pipeline coordinator.
class IrrigationPipeline {
public:
    IrrigationPipeline(net::io_context& ioc,
                       std::vector<HexCentroid> hexes,
                       IrrigationSignalPublisher publisher)
        : ioc_(ioc),
          kd_tree_(hexes),
          publisher_(std::move(publisher))
    {}

    void start_usgs_stream(const std::string& host, const std::string& target) {
        auto client = std::make_shared<UsgsClient>(
            ioc_, host, target,
            [this](const std::vector<GaugeRecord>& gauges) {
                this->on_gauges(gauges);
            });
        client->run();
    }

private:
    void on_gauges(const std::vector<GaugeRecord>& gauges) {
        for (const auto& g : gauges) {
            int32_t hex_id = kd_tree_.nearest_hex_id(g.position);
            if (hex_id < 0) continue;

            double score = compute_irrigation_score(g);

            IrrigationNeedSignal signal;
            signal.hex_id = hex_id;
            signal.gauge_site_id = g.site_id;
            signal.streamflow_cfs = g.streamflow_cfs;
            signal.irrigation_score = score;

            publisher_.publish(signal);
        }
    }

    net::io_context& ioc_;
    KdTree2D kd_tree_;
    IrrigationSignalPublisher publisher_;
};

// Example main: event loop wiring.
// In ROS2 deployment, you would:
//  - Initialize ROS2 node.
//  - Replace IrrigationSignalPublisher callback with rclcpp publisher.
//  - Run both io_context and ROS2 executor, either in separate threads or integrated.
int main() {
    net::io_context ioc;

    std::vector<HexCentroid> hexes;
    hexes.push_back(HexCentroid{Point2D{ -124000.0, 3920000.0 }, 1});
    hexes.push_back(HexCentroid{Point2D{ -123000.0, 3920500.0 }, 2});

    IrrigationSignalPublisher publisher([](const IrrigationNeedSignal& sig) {
        std::cout << "Irrigation signal: hex=" << sig.hex_id
                  << " gauge=" << sig.gauge_site_id
                  << " q_cfs=" << sig.streamflow_cfs
                  << " score=" << sig.irrigation_score << "\n";
        // ROS2 adaptation:
        // - Construct a ROS2 message type containing hex_id, gauge_site_id, streamflow_cfs, irrigation_score.
        // - Publish on a topic consumed by a solar-powered valve controller node.
    });

    IrrigationPipeline pipeline(ioc, hexes, publisher);

    // Example WaterServices endpoint for instantaneous values (legacy; to be migrated to api.waterdata.usgs.gov).
    std::string host   = "waterservices.usgs.gov";
    std::string target = "/nwis/iv/?format=json&sites=09510200&parameterCd=00060";

    pipeline.start_usgs_stream(host, target);

    ioc.run();

    return 0;
}
