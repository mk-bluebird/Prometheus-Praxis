// File: ecorestorationshard/src/cybow_codec.hpp

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct CybowWorkloadFrame {
    std::string sample_id;
    std::string node_id;
    std::int64_t timestamp_unix_s;
    double energy_req_j;
    double energy_surplus_j;
    float hydraulic_risk;
    float uncertainty_risk;
    float renergy;
    float rhydraulic;
    float runcertainty;
    double vt_before;
    double vt_after;
    double delta_vt;
    float kfactor;
    float efactor;
    float rfactor;
    std::string evidence_hex;
    std::string signing_hex;
    std::string logical_name;   // e.g. "WorkloadEnergyDeltaVt"
    std::string aln_anchor_hex; // e.g. "0xCYBOWPHX20260725"
};

class CybowCodec {
public:
    static std::vector<std::uint8_t> encode(const CybowWorkloadFrame &f) {
        std::vector<std::uint8_t> buf;
        auto push_u8  = [&buf](std::uint8_t v){ buf.push_back(v); };
        auto push_u16 = [&buf](std::uint16_t v){
            buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
            buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
        };
        auto push_u32 = [&buf](std::uint32_t v){
            buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
            buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
        };
        auto push_i64 = [&buf](std::int64_t v){
            for (int i = 7; i >= 0; --i) {
                buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
            }
        };
        auto push_f32 = [&buf](float v){
            static_assert(sizeof(float) == 4, "float must be 32-bit");
            std::uint32_t u;
            std::memcpy(&u, &v, sizeof(float));
            push_u32(u);
        };
        auto push_f64 = [&buf](double v){
            static_assert(sizeof(double) == 8, "double must be 64-bit");
            std::uint64_t u;
            std::memcpy(&u, &v, sizeof(double));
            for (int i = 7; i >= 0; --i) {
                buf.push_back(static_cast<std::uint8_t>((u >> (8 * i)) & 0xFF));
            }
        };
        auto push_bytes = [&buf](const std::string &s){
            buf.insert(buf.end(), s.begin(), s.end());
        };

        // Header: magic, version, placeholder frame_length.
        push_u32(0x43594257u); // 'CYBW'
        push_u16(0x0001u);
        std::size_t length_pos = buf.size();
        push_u32(0u); // to be filled later

        // ALN anchor and logical_name (truncated/padded).
        std::string anchor = f.aln_anchor_hex;
        if (anchor.size() > 16) anchor.resize(16);
        std::string logical = f.logical_name;
        if (logical.size() > 32) logical.resize(32);

        while (anchor.size() < 16) anchor.push_back('\0');
        while (logical.size() < 32) logical.push_back('\0');

        push_bytes(anchor);
        push_bytes(logical);

        // sample_id
        if (f.sample_id.size() > 255) {
            throw std::runtime_error("sample_id too long");
        }
        push_u8(static_cast<std::uint8_t>(f.sample_id.size()));
        push_bytes(f.sample_id);

        // node_id
        if (f.node_id.size() > 255) {
            throw std::runtime_error("node_id too long");
        }
        push_u8(static_cast<std::uint8_t>(f.node_id.size()));
        push_bytes(f.node_id);

        // timestamp and numeric payload
        push_i64(f.timestamp_unix_s);
        push_f64(f.energy_req_j);
        push_f64(f.energy_surplus_j);
        push_f32(f.hydraulic_risk);
        push_f32(f.uncertainty_risk);
        push_f32(f.renergy);
        push_f32(f.rhydraulic);
        push_f32(f.runcertainty);
        push_f64(f.vt_before);
        push_f64(f.vt_after);
        push_f64(f.delta_vt);
        push_f32(f.kfactor);
        push_f32(f.efactor);
        push_f32(f.rfactor);

        // evidence_hex
        if (f.evidence_hex.size() > 255) {
            throw std::runtime_error("evidence_hex too long");
        }
        push_u8(static_cast<std::uint8_t>(f.evidence_hex.size()));
        push_bytes(f.evidence_hex);

        // signing_hex
        if (f.signing_hex.size() > 255) {
            throw std::runtime_error("signing_hex too long");
        }
        push_u8(static_cast<std::uint8_t>(f.signing_hex.size()));
        push_bytes(f.signing_hex);

        // Fill frame_length_u32.
        std::uint32_t framelen = static_cast<std::uint32_t>(buf.size());
        buf[length_pos + 0] = static_cast<std::uint8_t>((framelen >> 24) & 0xFF);
        buf[length_pos + 1] = static_cast<std::uint8_t>((framelen >> 16) & 0xFF);
        buf[length_pos + 2] = static_cast<std::uint8_t>((framelen >> 8) & 0xFF);
        buf[length_pos + 3] = static_cast<std::uint8_t>(framelen & 0xFF);

        return buf;
    }

    static CybowWorkloadFrame decode(const std::vector<std::uint8_t> &buf) {
        auto read_u8 = [&buf](std::size_t &pos) -> std::uint8_t {
            if (pos >= buf.size()) throw std::runtime_error("buffer underflow");
            return buf[pos++];
        };
        auto read_u16 = [&buf,&read_u8](std::size_t &pos) -> std::uint16_t {
            std::uint16_t hi = read_u8(pos);
            std::uint16_t lo = read_u8(pos);
            return static_cast<std::uint16_t>((hi << 8) | lo);
        };
        auto read_u32 = [&buf,&read_u8](std::size_t &pos) -> std::uint32_t {
            std::uint32_t b0 = read_u8(pos);
            std::uint32_t b1 = read_u8(pos);
            std::uint32_t b2 = read_u8(pos);
            std::uint32_t b3 = read_u8(pos);
            return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        };
        auto read_i64 = [&buf,&read_u8](std::size_t &pos) -> std::int64_t {
            std::uint64_t v = 0;
            for (int i = 7; i >= 0; --i) {
                std::uint64_t b = read_u8(pos);
                v |= (b << (8 * i));
            }
            return static_cast<std::int64_t>(v);
        };
        auto read_f32 = [&buf,&read_u32](std::size_t &pos) -> float {
            std::uint32_t u = read_u32(pos);
            float v;
            std::memcpy(&v, &u, sizeof(float));
            return v;
        };
        auto read_f64 = [&buf,&read_u8](std::size_t &pos) -> double {
            std::uint64_t u = 0;
            for (int i = 7; i >= 0; --i) {
                std::uint64_t b = read_u8(pos);
                u |= (b << (8 * i));
            }
            double v;
            std::memcpy(&v, &u, sizeof(double));
            return v;
        };
        auto read_bytes = [&buf](std::size_t &pos, std::size_t n) -> std::string {
            if (pos + n > buf.size()) throw std::runtime_error("buffer underflow");
            std::string s(buf.begin() + static_cast<long>(pos),
                          buf.begin() + static_cast<long>(pos + n));
            pos += n;
            return s;
        };

        std::size_t pos = 0;
        std::uint32_t magic = read_u32(pos);
        if (magic != 0x43594257u) throw std::runtime_error("invalid magic");
        std::uint16_t version = read_u16(pos);
        if (version != 0x0001u) throw std::runtime_error("unsupported version");
        std::uint32_t framelen = read_u32(pos);
        if (framelen != buf.size()) {
            throw std::runtime_error("frame_length mismatch");
        }

        std::string anchor = read_bytes(pos, 16);
        std::string logical = read_bytes(pos, 32);
        // Trim null padding.
        anchor.erase(anchor.find_last_not_of('\0') + 1);
        logical.erase(logical.find_last_not_of('\0') + 1);

        CybowWorkloadFrame f;
        f.aln_anchor_hex = anchor;
        f.logical_name   = logical;

        std::uint8_t sid_len = read_u8(pos);
        f.sample_id = read_bytes(pos, sid_len);
        std::uint8_t nid_len = read_u8(pos);
        f.node_id = read_bytes(pos, nid_len);

        f.timestamp_unix_s = read_i64(pos);
        f.energy_req_j     = read_f64(pos);
        f.energy_surplus_j = read_f64(pos);
        f.hydraulic_risk   = read_f32(pos);
        f.uncertainty_risk = read_f32(pos);
        f.renergy          = read_f32(pos);
        f.rhydraulic       = read_f32(pos);
        f.runcertainty     = read_f32(pos);
        f.vt_before        = read_f64(pos);
        f.vt_after         = read_f64(pos);
        f.delta_vt         = read_f64(pos);
        f.kfactor          = read_f32(pos);
        f.efactor          = read_f32(pos);
        f.rfactor          = read_f32(pos);

        std::uint8_t evid_len = read_u8(pos);
        f.evidence_hex = read_bytes(pos, evid_len);
        std::uint8_t sign_len = read_u8(pos);
        f.signing_hex  = read_bytes(pos, sign_len);

        return f;
    }
};
