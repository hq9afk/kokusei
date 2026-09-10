#pragma once

// canvas sizing
constexpr float kVisualizerCanvasFraction = 0.8f;
constexpr int kVisualizerCanvasMin = 200;

// bar visualizer
constexpr float kVisualizerBarWidth = 10.0f;
constexpr float kVisualizerBarSpacing = 7.0f;
constexpr float kVisualizerBarRadius = 3.0f;
constexpr float kVisualizerBarHeightRatio = 0.7f;
constexpr float kVisualizerBarOpacity = 0.6f;
constexpr float kVisualizerBarMinHeight = 5.0f;
constexpr int kVisualizerBarFps = 60;

// default window size
constexpr int kVisualizerDefaultWindow = 1000;

// audio capture
constexpr unsigned int kVisualizerSampleRate = 11000;
constexpr int kVisualizerChannels = 2;
constexpr int kVisualizerSampleSize = 1024;
constexpr int kVisualizerFragmentSize = 4096;
constexpr float kVisualizerFftScale = 10.2f;
constexpr float kVisualizerFftCutOff = 0.3f;

// gravity & smoothing
constexpr int kVisualizerGravityAverageFrames = 5;
constexpr float kVisualizerGravityStep = 4.2f;
constexpr float kVisualizerSampleHybridWeight = 0.065f;
constexpr int kVisualizerSampleMode = 0;
constexpr int kVisualizerAdjacentSampleNums = 1;
constexpr float kVisualizerSampleRange = 0.9f;
constexpr float kVisualizerSampleScale = 8.0f;
constexpr float kVisualizerSmoothFactor = 0.025f;
constexpr int kVisualizerFps = 60;

// clamp ranges
constexpr int kVisualizerFpsMin = 15;
constexpr int kVisualizerFpsMax = 144;
constexpr float kVisualizerParticleThinMin = 0.0f;
constexpr float kVisualizerParticleThinMax = 0.95f;
constexpr int kVisualizerParticleSizeMin = 1;
constexpr int kVisualizerParticleSizeMax = 4;
constexpr int kVisualizerComplexityMin = 1;
constexpr int kVisualizerComplexityMax = 3;
constexpr float kVisualizerGlowDirectionsMin = 4.0f;
constexpr float kVisualizerGlowDirectionsMax = 32.0f;
constexpr float kVisualizerGlowQualityMin = 2.0f;
constexpr float kVisualizerGlowQualityMax = 8.0f;

enum class VisualizerShape { Bar, Sphere };

struct VisualizerParams {
    VisualizerShape visualizer_shape = VisualizerShape::Bar;
    int fps = kVisualizerFps;
    float particle_thin = 0.12f;
    int particle_size = 4;
    int fractal_complexity = 3;
    float glow_directions = 16.0f;
    float glow_quality = 6.0f;
};
