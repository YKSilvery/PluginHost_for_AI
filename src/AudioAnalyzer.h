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

    // =========================================================================
    // STFT (Short-Time Fourier Transform) — v1.1
    // =========================================================================
    struct STFTFrame
    {
        float                timeSec = 0.0f;       // center time of this frame
        std::vector<float>   magnitudeDB;           // dB per bin (fftSize/2+1)
    };

    struct STFTResult
    {
        int                      fftSize            = 0;
        int                      hopSize            = 0;
        float                    frequencyResolution = 0.0f;
        float                    timeResolution     = 0.0f;  // seconds per frame
        int                      numFrames          = 0;
        int                      numBins            = 0;
        std::vector<STFTFrame>   frames;
        // Per-band energy summary (low/mid/high)
        float                    energyLowDB  = -120.0f;  // 20-300 Hz
        float                    energyMidDB  = -120.0f;  // 300-4000 Hz
        float                    energyHighDB = -120.0f;  // 4000-20000 Hz
    };

    // =========================================================================
    // Time-domain analysis — v1.1
    // =========================================================================
    struct TimeDomainResult
    {
        int                      numSamples     = 0;
        int                      numChannels    = 0;
        double                   sampleRate     = 0.0;
        float                    durationMs     = 0.0f;
        // Per-channel sample data (optionally downsampled)
        std::vector<std::vector<float>> channelSamples;
        // Per-channel stats
        std::vector<ChannelAnalysis>    channelStats;
        // Zero-crossing rate per channel
        std::vector<float>              zeroCrossingRates;
        // First non-zero sample index per channel (latency detection)
        std::vector<int>                firstNonZeroSample;
    };

    // =========================================================================
    // Loudness / Envelope analysis — v1.1
    // =========================================================================
    struct EnvelopePoint
    {
        float timeSec  = 0.0f;
        float rmsDB    = -120.0f;
        float peakDB   = -120.0f;
    };

    struct LoudnessResult
    {
        int                          numFrames      = 0;
        float                        windowMs       = 0.0f;  // analysis window size
        float                        hopMs          = 0.0f;  // hop between frames
        float                        overallRmsDB   = -120.0f;
        float                        overallPeakDB  = -120.0f;
        float                        dynamicRangeDB = 0.0f;   // peak - min_rms
        float                        crestFactorDB  = 0.0f;   // peak - rms
        // Attack/release detection
        float                        attackTimeMs   = 0.0f;   // time to reach -3dB of peak
        float                        releaseTimeMs  = 0.0f;   // time from peak to -20dB below peak
        // Per-frame envelope
        std::vector<EnvelopePoint>   envelope;
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

    // =========================================================================
    // New analysis methods — v1.1
    // =========================================================================

    /** Perform STFT on an audio buffer. Returns time-frequency representation. */
    static STFTResult performSTFT (const juce::AudioBuffer<float>& buffer,
                                   double sampleRate,
                                   int fftOrder = 11,    // 2048 points
                                   int hopSize  = 512,
                                   int channel  = 0);

    /** Convert STFT result to JSON. */
    static juce::var stftToJson (const STFTResult& result,
                                 bool includeFullSpectrogram = false,
                                 int maxFrames = 200);

    /** Extract time-domain signal data with optional downsampling. */
    static TimeDomainResult analyzeTimeDomain (const juce::AudioBuffer<float>& buffer,
                                               double sampleRate,
                                               int maxSamplesPerChannel = 10000);

    /** Convert time-domain result to JSON. */
    static juce::var timeDomainToJson (const TimeDomainResult& result,
                                       bool includeSamples = true);

    /** Analyze loudness envelope over time (windowed RMS/peak). */
    static LoudnessResult analyzeLoudness (const juce::AudioBuffer<float>& buffer,
                                           double sampleRate,
                                           float windowMs = 10.0f,
                                           float hopMs    = 5.0f,
                                           int channel    = 0);

    /** Convert loudness result to JSON. */
    static juce::var loudnessToJson (const LoudnessResult& result,
                                     bool includeEnvelope = true,
                                     int maxPoints = 500);

private:
    AudioAnalyzer() = delete;
};
