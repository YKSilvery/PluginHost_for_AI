#include "AudioAnalyzer.h"
#include "JsonHelper.h"
#include <algorithm>
#include <cmath>

// ============================================================================
AudioAnalyzer::ChannelAnalysis AudioAnalyzer::analyzeChannel (const float* data, int numSamples)
{
    ChannelAnalysis ca;
    if (numSamples <= 0 || data == nullptr)
        return ca;

    double sumSquares = 0.0;
    double sum        = 0.0;
    float  maxAbs     = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float s = data[i];

        if (std::isnan (s)) { ca.hasNaN = true; ++ca.nanCount; continue; }
        if (std::isinf (s)) { ca.hasInf = true; ++ca.infCount; continue; }

        sum += static_cast<double> (s);
        sumSquares += static_cast<double> (s) * static_cast<double> (s);
        const float a = std::fabs (s);
        if (a > maxAbs)
            maxAbs = a;
    }

    const int validCount = numSamples - ca.nanCount - ca.infCount;
    if (validCount > 0)
    {
        ca.rms      = static_cast<float> (std::sqrt (sumSquares / validCount));
        ca.rmsDB    = ca.rms > 0.0f ? 20.0f * std::log10 (ca.rms) : -120.0f;
        ca.peak     = maxAbs;
        ca.peakDB   = maxAbs > 0.0f ? 20.0f * std::log10 (maxAbs) : -120.0f;
        ca.dcOffset = static_cast<float> (sum / validCount);
    }
    return ca;
}

// ============================================================================
AudioAnalyzer::FFTResult AudioAnalyzer::performFFT (const float* data, int numSamples,
                                                    double sampleRate, int fftOrder,
                                                    int maxPeaks)
{
    FFTResult result;
    const int fftSize = 1 << fftOrder;      // e.g. 4096
    result.fftSize = fftSize;
    result.frequencyResolution = static_cast<float> (sampleRate / fftSize);

    if (numSamples <= 0 || data == nullptr)
        return result;

    // Prepare FFT input — zero-pad or truncate
    std::vector<float> fftData (static_cast<size_t> (fftSize * 2), 0.0f);
    const int copyLen = std::min (numSamples, fftSize);
    std::copy (data, data + copyLen, fftData.begin());

    // Apply Hann window
    juce::dsp::WindowingFunction<float> window (static_cast<size_t> (fftSize),
                                                juce::dsp::WindowingFunction<float>::hann);
    window.multiplyWithWindowingTable (fftData.data(), static_cast<size_t> (fftSize));

    // Perform FFT
    juce::dsp::FFT fft (fftOrder);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // Extract magnitude spectrum (fftSize/2 + 1 bins)
    const int numBins = fftSize / 2 + 1;
    result.magnitudeSpectrum.resize (static_cast<size_t> (numBins));
    for (int i = 0; i < numBins; ++i)
    {
        float mag = fftData[static_cast<size_t> (i)];
        // Normalize
        mag /= static_cast<float> (fftSize);
        // Convert to dB
        result.magnitudeSpectrum[static_cast<size_t> (i)] =
            mag > 1.0e-10f ? 20.0f * std::log10 (mag) : -200.0f;
    }

    // Detect peaks (local maxima above -80 dB)
    const float peakThreshold = -80.0f;
    struct PeakCandidate { float freq; float magDB; };
    std::vector<PeakCandidate> candidates;

    for (int i = 1; i < numBins - 1; ++i)
    {
        float m  = result.magnitudeSpectrum[static_cast<size_t> (i)];
        float ml = result.magnitudeSpectrum[static_cast<size_t> (i - 1)];
        float mr = result.magnitudeSpectrum[static_cast<size_t> (i + 1)];

        if (m > ml && m > mr && m > peakThreshold)
        {
            float freq = static_cast<float> (i) * result.frequencyResolution;
            candidates.push_back ({ freq, m });
        }
    }

    // Sort by magnitude descending and keep top N
    std::sort (candidates.begin(), candidates.end(),
               [] (const PeakCandidate& a, const PeakCandidate& b) { return a.magDB > b.magDB; });

    const int count = std::min (static_cast<int> (candidates.size()), maxPeaks);
    for (int i = 0; i < count; ++i)
        result.peaks.push_back ({ candidates[static_cast<size_t> (i)].freq,
                                  candidates[static_cast<size_t> (i)].magDB });

    return result;
}

// ============================================================================
AudioAnalyzer::AnalysisResult AudioAnalyzer::analyze (const juce::AudioBuffer<float>& buffer,
                                                      double sampleRate,
                                                      int fftOrder, int maxPeaks)
{
    AnalysisResult result;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
        result.channels.push_back (analyzeChannel (buffer.getReadPointer (ch), numSamples));

    // FFT from channel 0
    if (numChannels > 0 && numSamples > 0)
        result.fft = performFFT (buffer.getReadPointer (0), numSamples, sampleRate, fftOrder, maxPeaks);

    return result;
}

// ============================================================================
juce::var AudioAnalyzer::toJson (const AnalysisResult& result, bool includeFullSpectrum)
{
    using namespace JsonHelper;

    auto root = makeObject();

    // Per-channel analysis
    auto channelsArr = makeArray();
    for (size_t i = 0; i < result.channels.size(); ++i)
    {
        const auto& ch = result.channels[i];
        auto chObj = makeObject();
        set (chObj, "channel",   static_cast<int> (i));
        set (chObj, "rms",       ch.rms);
        set (chObj, "rms_db",    ch.rmsDB);
        set (chObj, "peak",      ch.peak);
        set (chObj, "peak_db",   ch.peakDB);
        set (chObj, "dc_offset", ch.dcOffset);
        set (chObj, "has_nan",   ch.hasNaN);
        set (chObj, "has_inf",   ch.hasInf);
        set (chObj, "nan_count", ch.nanCount);
        set (chObj, "inf_count", ch.infCount);
        append (channelsArr, chObj);
    }
    set (root, "channels", channelsArr);

    // FFT
    auto fftObj = makeObject();
    set (fftObj, "fft_size",             result.fft.fftSize);
    set (fftObj, "frequency_resolution", result.fft.frequencyResolution);

    // Peaks
    auto peaksArr = makeArray();
    for (const auto& p : result.fft.peaks)
    {
        auto pObj = makeObject();
        set (pObj, "frequency",    p.frequency);
        set (pObj, "magnitude_db", p.magnitudeDB);
        append (peaksArr, pObj);
    }
    set (fftObj, "peaks", peaksArr);

    // Full spectrum (optional, can be large)
    if (includeFullSpectrum && ! result.fft.magnitudeSpectrum.empty())
    {
        auto specArr = makeArray();
        for (auto v : result.fft.magnitudeSpectrum)
            append (specArr, v);
        set (fftObj, "spectrum_db", specArr);
    }

    set (root, "fft", fftObj);
    return root;
}
