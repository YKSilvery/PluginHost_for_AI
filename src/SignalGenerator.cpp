#include "SignalGenerator.h"

juce::AudioBuffer<float> SignalGenerator::generateImpulse (int numChannels, int numSamples,
                                                           float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.setSample (ch, 0, amplitude);
    return buffer;
}

juce::AudioBuffer<float> SignalGenerator::generateStep (int numChannels, int numSamples,
                                                        float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), amplitude, numSamples);
    return buffer;
}

juce::AudioBuffer<float> SignalGenerator::generateSineWave (int numChannels, int numSamples,
                                                            double sampleRate, float frequency,
                                                            float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    const double phaseIncrement = juce::MathConstants<double>::twoPi * frequency / sampleRate;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        double phase = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            data[i] = amplitude * static_cast<float> (std::sin (phase));
            phase += phaseIncrement;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
    }
    return buffer;
}

juce::AudioBuffer<float> SignalGenerator::generateSineSweep (int numChannels, int numSamples,
                                                             double sampleRate,
                                                             float startFreq, float endFreq,
                                                             float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    // Linear chirp: f(t) = f0 + (f1 - f0) * t / T
    // phase(t) = 2π * [f0 * t + (f1 - f0) * t² / (2T)]
    const double T = static_cast<double> (numSamples) / sampleRate;
    const double f0 = static_cast<double> (startFreq);
    const double f1 = static_cast<double> (endFreq);
    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            double t = static_cast<double> (i) / sampleRate;
            double phase = twoPi * (f0 * t + (f1 - f0) * t * t / (2.0 * T));
            data[i] = amplitude * static_cast<float> (std::sin (phase));
        }
    }
    return buffer;
}

juce::AudioBuffer<float> SignalGenerator::generateWhiteNoise (int numChannels, int numSamples,
                                                              float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    juce::Random rng;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = amplitude * (rng.nextFloat() * 2.0f - 1.0f);
    }
    return buffer;
}

juce::AudioBuffer<float> SignalGenerator::loadAudioFile (const juce::String& filePath)
{
    juce::File file (filePath);
    if (! file.existsAsFile())
    {
        DBG ("SignalGenerator::loadAudioFile - file not found: " + filePath);
        return {};
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        DBG ("SignalGenerator::loadAudioFile - unsupported format: " + filePath);
        return {};
    }

    const int numChannels = static_cast<int> (reader->numChannels);
    const int numSamples  = static_cast<int> (reader->lengthInSamples);

    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    reader->read (&buffer, 0, numSamples, 0, true, true);
    return buffer;
}

juce::AudioBuffer<float> SignalGenerator::fromJson (const juce::var& params, double sampleRate,
                                                    int numChannels)
{
    const juce::String type = params.getProperty ("signal_type", "silence").toString().toLowerCase();
    const float amplitude   = static_cast<float> (params.getProperty ("amplitude", 1.0));
    const double durationMs = static_cast<double> (params.getProperty ("duration_ms", 1000.0));
    const int numSamples    = static_cast<int> (sampleRate * durationMs / 1000.0);

    if (numSamples <= 0)
        return juce::AudioBuffer<float> (numChannels, 0);

    if (type == "impulse")
        return generateImpulse (numChannels, numSamples, amplitude);

    if (type == "step")
        return generateStep (numChannels, numSamples, amplitude);

    if (type == "sine" || type == "sine_wave")
    {
        float freq = static_cast<float> (params.getProperty ("frequency", 440.0));
        return generateSineWave (numChannels, numSamples, sampleRate, freq, amplitude);
    }

    if (type == "sine_sweep" || type == "sweep")
    {
        float startFreq = static_cast<float> (params.getProperty ("start_freq", 20.0));
        float endFreq   = static_cast<float> (params.getProperty ("end_freq", 20000.0));
        return generateSineSweep (numChannels, numSamples, sampleRate, startFreq, endFreq, amplitude);
    }

    if (type == "white_noise" || type == "noise")
        return generateWhiteNoise (numChannels, numSamples, amplitude);

    if (type == "wav_file" || type == "audio_file" || type == "file")
    {
        juce::String filePath = params.getProperty ("file_path", "").toString();
        if (filePath.isNotEmpty())
            return loadAudioFile (filePath);
    }

    // Default: silence
    juce::AudioBuffer<float> silence (numChannels, numSamples);
    silence.clear();
    return silence;
}
