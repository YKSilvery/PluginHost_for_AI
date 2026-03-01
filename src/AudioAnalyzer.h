#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

/**
 * Audio signal analyzer — extracts key features from audio buffers
 * and serializes results as JSON for AI agent consumption.
 */
class AudioAnalyzer
{
public:
    struct ChannelAnalysis
    {
        float rms        = 0.0f;
        float rmsDB      = -std::numeric_limits<float>::infinity();
        float peak       = 0.0f;
        float peakDB     = -std::numeric_limits<float>::infinity();
        float dcOffset   = 0.0f;
        bool  hasNaN     = false;
        bool  hasInf     = false;
        int   nanCount   = 0;
        int   infCount   = 0;
    };

    struct FFTPeak
    {
        float frequency   = 0.0f;
        float magnitudeDB = -std::numeric_limits<float>::infinity();
    };

    struct FFTResult
    {
        int                  fftSize             = 0;
        float                frequencyResolution = 0.0f;
        std::vector<float>   magnitudeSpectrum;   // dB, fftSize/2+1 bins
        std::vector<FFTPeak> peaks;               // sorted by magnitude desc
    };

    struct AnalysisResult
    {
        std::vector<ChannelAnalysis> channels;
        FFTResult                    fft;          // computed from channel 0
    };

    /** Analyze a single channel of raw samples. */
    static ChannelAnalysis analyzeChannel (const float* data, int numSamples);

    /** Perform FFT on raw samples, return magnitude spectrum and detected peaks. */
    static FFTResult performFFT (const float* data, int numSamples,
                                 double sampleRate, int fftOrder = 12,
                                 int maxPeaks = 10);

    /** Full analysis on an AudioBuffer. */
    static AnalysisResult analyze (const juce::AudioBuffer<float>& buffer,
                                   double sampleRate,
                                   int fftOrder = 12,
                                   int maxPeaks = 10);

    /** Convert analysis result to JSON var. */
    static juce::var toJson (const AnalysisResult& result,
                             bool includeFullSpectrum = false);

private:
    AudioAnalyzer() = delete;
};
