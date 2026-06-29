#pragma once
#include <cstdint>

struct EcoState2 {
    float x1;      // e.g., tank level, panel angle error
    float x2;      // e.g., moisture, SOC error
};

struct EcoCommand {
    float u;       // scalar actuator command, normalized -1..1 or 0..1
};

struct EcoSetpoint2 {
    float x1_ref;
    float x2_ref;
};

struct LyapunovConfig2 {
    float p1;      // positive weight for (x1 - x1_ref)^2
    float p2;      // positive weight for (x2 - x2_ref)^2
};

struct Corridor2 {
    float x1_min;
    float x1_max;
    float x2_min;
    float x2_max;
    float u_min;
    float u_max;
};
