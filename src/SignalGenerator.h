#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

/**
 * Multi-source test signal generator.
 * All methods return AudioBuffer<float> ready for injection into a plugin.
 */
class SignalGenerator
{
public:
    /** Impulse (Dirac delta): single sample = amplitude at sample 0, rest = 0. */
    static juce::AudioBuffer<float> generateImpulse (int numChannels, int numSamples,
                                                     float amplitude = 1.0f);

    /** Unit step: all samples = amplitude from sample 0 onward. */
    static juce::AudioBuffer<float> generateStep (int numChannels, int numSamples,
                                                  float amplitude = 1.0f);

    /** Sine wave at a fixed frequency. */
    static juce::AudioBuffer<float> generateSineWave (int numChannels, int numSamples,
                                                      double sampleRate, float frequency,
                                                      float amplitude = 1.0f);

    /** Linear sine sweep from startFreq to endFreq. */
    static juce::AudioBuffer<float> generateSineSweep (int numChannels, int numSamples,
                                                       double sampleRate,
                                                       float startFreq, float endFreq,
                                                       float amplitude = 1.0f);

    /** White noise (uniform distribution). */
    static juce::AudioBuffer<float> generateWhiteNoise (int numChannels, int numSamples,
                                                        float amplitude = 1.0f);

    /**
     * Load an external WAV / AIFF / FLAC file.
     * If targetSampleRate > 0 and differs from the file's rate, no resampling is done —
     * the caller should be aware of the mismatch.
     */
    static juce::AudioBuffer<float> loadAudioFile (const juce::String& filePath);

    /** Create a signal buffer from a JSON command description. */
    static juce::AudioBuffer<float> fromJson (const juce::var& params, double sampleRate,
                                              int numChannels);

private:
    SignalGenerator() = delete;
};
