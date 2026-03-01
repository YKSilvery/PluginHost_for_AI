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

// ============================================================================
// STFT — Short-Time Fourier Transform (v1.1)
// ============================================================================
AudioAnalyzer::STFTResult AudioAnalyzer::performSTFT (const juce::AudioBuffer<float>& buffer,
                                                      double sampleRate,
                                                      int fftOrder, int hopSize,
                                                      int channel)
{
    STFTResult result;
    const int fftSize = 1 << fftOrder;
    result.fftSize = fftSize;
    result.hopSize = hopSize;
    result.numBins = fftSize / 2 + 1;
    result.frequencyResolution = static_cast<float> (sampleRate / fftSize);
    result.timeResolution      = static_cast<float> (hopSize / sampleRate);

    if (buffer.getNumSamples() == 0 || channel >= buffer.getNumChannels())
        return result;

    const float* data = buffer.getReadPointer (channel);
    const int numSamples = buffer.getNumSamples();

    // Create windowing function
    juce::dsp::WindowingFunction<float> window (static_cast<size_t> (fftSize),
                                                juce::dsp::WindowingFunction<float>::hann);
    juce::dsp::FFT fft (fftOrder);

    // Accumulate band energies
    double energyLow  = 0.0, energyMid  = 0.0, energyHigh = 0.0;
    int    countLow   = 0,   countMid   = 0,   countHigh  = 0;

    for (int frameStart = 0; frameStart + fftSize <= numSamples; frameStart += hopSize)
    {
        STFTFrame frame;
        frame.timeSec = static_cast<float> ((frameStart + fftSize / 2) / sampleRate);

        // Copy and window
        std::vector<float> fftData (static_cast<size_t> (fftSize * 2), 0.0f);
        std::copy (data + frameStart, data + frameStart + fftSize, fftData.begin());
        window.multiplyWithWindowingTable (fftData.data(), static_cast<size_t> (fftSize));

        // FFT
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        // Extract magnitude in dB
        frame.magnitudeDB.resize (static_cast<size_t> (result.numBins));
        for (int i = 0; i < result.numBins; ++i)
        {
            float mag = fftData[static_cast<size_t> (i)] / static_cast<float> (fftSize);
            float db = mag > 1.0e-10f ? 20.0f * std::log10 (mag) : -200.0f;
            frame.magnitudeDB[static_cast<size_t> (i)] = db;

            // Accumulate band energy (linear power)
            if (mag > 1.0e-10f)
            {
                float freq = i * result.frequencyResolution;
                double power = static_cast<double> (mag) * mag;
                if (freq >= 20.0f && freq < 300.0f)       { energyLow  += power; ++countLow; }
                else if (freq >= 300.0f && freq < 4000.0f) { energyMid  += power; ++countMid; }
                else if (freq >= 4000.0f)                  { energyHigh += power; ++countHigh; }
            }
        }

        result.frames.push_back (std::move (frame));
    }

    result.numFrames = static_cast<int> (result.frames.size());

    // Compute band energy summaries
    if (countLow > 0)
        result.energyLowDB  = static_cast<float> (10.0 * std::log10 (energyLow / countLow));
    if (countMid > 0)
        result.energyMidDB  = static_cast<float> (10.0 * std::log10 (energyMid / countMid));
    if (countHigh > 0)
        result.energyHighDB = static_cast<float> (10.0 * std::log10 (energyHigh / countHigh));

    return result;
}

// ============================================================================
juce::var AudioAnalyzer::stftToJson (const STFTResult& result,
                                     bool includeFullSpectrogram,
                                     int maxFrames)
{
    using namespace JsonHelper;

    auto root = makeObject();
    set (root, "fft_size",              result.fftSize);
    set (root, "hop_size",              result.hopSize);
    set (root, "num_frames",            result.numFrames);
    set (root, "num_bins",              result.numBins);
    set (root, "frequency_resolution",  result.frequencyResolution);
    set (root, "time_resolution_sec",   result.timeResolution);
    set (root, "energy_low_db",         result.energyLowDB);
    set (root, "energy_mid_db",         result.energyMidDB);
    set (root, "energy_high_db",        result.energyHighDB);

    // Per-frame summary (always included: time + max magnitude)
    auto summaryArr = makeArray();
    // Downsample frames if too many
    int step = std::max (1, result.numFrames / maxFrames);
    for (int i = 0; i < result.numFrames; i += step)
    {
        const auto& frame = result.frames[static_cast<size_t> (i)];
        auto fObj = makeObject();
        set (fObj, "time_sec", frame.timeSec);

        // Max magnitude in this frame
        float maxMag = -200.0f;
        float maxFreq = 0.0f;
        for (int b = 1; b < result.numBins; ++b)
        {
            if (frame.magnitudeDB[static_cast<size_t> (b)] > maxMag)
            {
                maxMag = frame.magnitudeDB[static_cast<size_t> (b)];
                maxFreq = b * result.frequencyResolution;
            }
        }
        set (fObj, "peak_magnitude_db", maxMag);
        set (fObj, "peak_frequency_hz", maxFreq);
        append (summaryArr, fObj);
    }
    set (root, "frame_summary", summaryArr);

    // Full spectrogram (optional, can be very large)
    if (includeFullSpectrogram)
    {
        auto spectrogramArr = makeArray();
        for (int i = 0; i < result.numFrames; i += step)
        {
            const auto& frame = result.frames[static_cast<size_t> (i)];
            auto binArr = makeArray();
            for (auto v : frame.magnitudeDB)
                append (binArr, v);
            append (spectrogramArr, binArr);
        }
        set (root, "spectrogram_db", spectrogramArr);
    }

    return root;
}

// ============================================================================
// Time-domain analysis (v1.1)
// ============================================================================
AudioAnalyzer::TimeDomainResult AudioAnalyzer::analyzeTimeDomain (
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    int maxSamplesPerChannel)
{
    TimeDomainResult result;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    result.numSamples  = numSamples;
    result.numChannels = numChannels;
    result.sampleRate  = sampleRate;
    result.durationMs  = static_cast<float> (numSamples / sampleRate * 1000.0);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);

        // Channel stats
        result.channelStats.push_back (analyzeChannel (data, numSamples));

        // Zero-crossing rate
        int crossings = 0;
        for (int i = 1; i < numSamples; ++i)
        {
            if ((data[i] >= 0.0f && data[i - 1] < 0.0f) ||
                (data[i] < 0.0f && data[i - 1] >= 0.0f))
                ++crossings;
        }
        float zcr = numSamples > 1
            ? static_cast<float> (crossings) / static_cast<float> (numSamples - 1)
            : 0.0f;
        result.zeroCrossingRates.push_back (zcr);

        // First non-zero sample (latency detection)
        int firstNZ = -1;
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::fabs (data[i]) > 1.0e-7f)
            {
                firstNZ = i;
                break;
            }
        }
        result.firstNonZeroSample.push_back (firstNZ);

        // Downsample for JSON output
        std::vector<float> samples;
        if (numSamples <= maxSamplesPerChannel)
        {
            samples.assign (data, data + numSamples);
        }
        else
        {
            // Pick evenly spaced samples
            samples.reserve (static_cast<size_t> (maxSamplesPerChannel));
            for (int i = 0; i < maxSamplesPerChannel; ++i)
            {
                int idx = static_cast<int> (
                    static_cast<double> (i) * (numSamples - 1) / (maxSamplesPerChannel - 1));
                samples.push_back (data[idx]);
            }
        }
        result.channelSamples.push_back (std::move (samples));
    }

    return result;
}

// ============================================================================
juce::var AudioAnalyzer::timeDomainToJson (const TimeDomainResult& result,
                                            bool includeSamples)
{
    using namespace JsonHelper;

    auto root = makeObject();
    set (root, "num_samples",  result.numSamples);
    set (root, "num_channels", result.numChannels);
    set (root, "sample_rate",  result.sampleRate);
    set (root, "duration_ms",  result.durationMs);

    auto channelsArr = makeArray();
    for (int ch = 0; ch < result.numChannels; ++ch)
    {
        auto chObj = makeObject();
        set (chObj, "channel", ch);

        // Stats
        const auto& stats = result.channelStats[static_cast<size_t> (ch)];
        set (chObj, "rms_db",       stats.rmsDB);
        set (chObj, "peak_db",      stats.peakDB);
        set (chObj, "dc_offset",    stats.dcOffset);
        set (chObj, "has_nan",      stats.hasNaN);
        set (chObj, "has_inf",      stats.hasInf);

        set (chObj, "zero_crossing_rate", result.zeroCrossingRates[static_cast<size_t> (ch)]);
        set (chObj, "first_nonzero_sample", result.firstNonZeroSample[static_cast<size_t> (ch)]);

        if (result.firstNonZeroSample[static_cast<size_t> (ch)] >= 0)
        {
            float latencyMs = static_cast<float> (
                result.firstNonZeroSample[static_cast<size_t> (ch)] / result.sampleRate * 1000.0);
            set (chObj, "latency_ms", latencyMs);
        }

        // Sample data
        if (includeSamples)
        {
            auto samplesArr = makeArray();
            for (auto v : result.channelSamples[static_cast<size_t> (ch)])
                append (samplesArr, v);
            set (chObj, "samples", samplesArr);
            set (chObj, "samples_count",
                 static_cast<int> (result.channelSamples[static_cast<size_t> (ch)].size()));
        }

        append (channelsArr, chObj);
    }
    set (root, "channels", channelsArr);

    return root;
}

// ============================================================================
// Loudness / Envelope analysis (v1.1)
// ============================================================================
AudioAnalyzer::LoudnessResult AudioAnalyzer::analyzeLoudness (
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    float windowMs, float hopMs,
    int channel)
{
    LoudnessResult result;
    result.windowMs = windowMs;
    result.hopMs    = hopMs;

    if (buffer.getNumSamples() == 0 || channel >= buffer.getNumChannels())
        return result;

    const float* data = buffer.getReadPointer (channel);
    const int numSamples = buffer.getNumSamples();

    const int windowSamples = static_cast<int> (sampleRate * windowMs / 1000.0);
    const int hopSamples    = static_cast<int> (sampleRate * hopMs / 1000.0);

    if (windowSamples <= 0 || hopSamples <= 0)
        return result;

    // Overall stats
    auto overallStats = analyzeChannel (data, numSamples);
    result.overallRmsDB  = overallStats.rmsDB;
    result.overallPeakDB = overallStats.peakDB;
    result.crestFactorDB = overallStats.peakDB - overallStats.rmsDB;

    // Compute per-frame envelope
    float minRmsDB = 0.0f;
    float maxRmsDB = -200.0f;
    float peakRmsDB = -200.0f;
    int   peakFrameIdx = 0;

    for (int start = 0; start + windowSamples <= numSamples; start += hopSamples)
    {
        EnvelopePoint pt;
        pt.timeSec = static_cast<float> ((start + windowSamples / 2) / sampleRate);

        // RMS in this window
        double sumSq = 0.0;
        float  peak  = 0.0f;
        for (int i = start; i < start + windowSamples; ++i)
        {
            float s = data[i];
            sumSq += static_cast<double> (s) * s;
            float a = std::fabs (s);
            if (a > peak) peak = a;
        }
        float rms = static_cast<float> (std::sqrt (sumSq / windowSamples));
        pt.rmsDB  = rms > 1.0e-10f  ? 20.0f * std::log10 (rms)  : -120.0f;
        pt.peakDB = peak > 1.0e-10f ? 20.0f * std::log10 (peak) : -120.0f;

        if (pt.rmsDB > maxRmsDB)
        {
            maxRmsDB = pt.rmsDB;
            peakFrameIdx = static_cast<int> (result.envelope.size());
        }
        if (pt.rmsDB < minRmsDB && pt.rmsDB > -119.0f)
            minRmsDB = pt.rmsDB;

        if (pt.rmsDB > peakRmsDB)
            peakRmsDB = pt.rmsDB;

        result.envelope.push_back (pt);
    }

    result.numFrames = static_cast<int> (result.envelope.size());
    result.dynamicRangeDB = maxRmsDB - minRmsDB;

    // Attack time: time from start to first frame reaching within -3dB of peak
    float attackThreshold = peakRmsDB - 3.0f;
    result.attackTimeMs = 0.0f;
    for (int i = 0; i < result.numFrames; ++i)
    {
        if (result.envelope[static_cast<size_t> (i)].rmsDB >= attackThreshold)
        {
            result.attackTimeMs = result.envelope[static_cast<size_t> (i)].timeSec * 1000.0f;
            break;
        }
    }

    // Release time: time from peak frame to first frame dropping below peak - 20dB
    float releaseThreshold = peakRmsDB - 20.0f;
    result.releaseTimeMs = 0.0f;
    for (int i = peakFrameIdx + 1; i < result.numFrames; ++i)
    {
        if (result.envelope[static_cast<size_t> (i)].rmsDB <= releaseThreshold)
        {
            result.releaseTimeMs =
                (result.envelope[static_cast<size_t> (i)].timeSec
                 - result.envelope[static_cast<size_t> (peakFrameIdx)].timeSec) * 1000.0f;
            break;
        }
    }

    return result;
}

// ============================================================================
juce::var AudioAnalyzer::loudnessToJson (const LoudnessResult& result,
                                          bool includeEnvelope,
                                          int maxPoints)
{
    using namespace JsonHelper;

    auto root = makeObject();
    set (root, "num_frames",       result.numFrames);
    set (root, "window_ms",        result.windowMs);
    set (root, "hop_ms",           result.hopMs);
    set (root, "overall_rms_db",   result.overallRmsDB);
    set (root, "overall_peak_db",  result.overallPeakDB);
    set (root, "dynamic_range_db", result.dynamicRangeDB);
    set (root, "crest_factor_db",  result.crestFactorDB);
    set (root, "attack_time_ms",   result.attackTimeMs);
    set (root, "release_time_ms",  result.releaseTimeMs);

    if (includeEnvelope && ! result.envelope.empty())
    {
        auto envArr = makeArray();
        int step = std::max (1, result.numFrames / maxPoints);
        for (int i = 0; i < result.numFrames; i += step)
        {
            const auto& pt = result.envelope[static_cast<size_t> (i)];
            auto ptObj = makeObject();
            set (ptObj, "time_sec", pt.timeSec);
            set (ptObj, "rms_db",   pt.rmsDB);
            set (ptObj, "peak_db",  pt.peakDB);
            append (envArr, ptObj);
        }
        set (root, "envelope", envArr);
    }

    return root;
}
