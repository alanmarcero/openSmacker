#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>

TEST_CASE("OpenSmackerProcessor instantiates with valid parameters", "[plugin]") {
    OpenSmackerProcessor processor;
    REQUIRE(processor.getName() == "openSmacker");
    REQUIRE(processor.acceptsMidi());
    REQUIRE_FALSE(processor.producesMidi());

    REQUIRE(processor.parameters.getParameter(opensmacker::params::kPreBursts) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kFlamSpread) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kVoiceMode) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kFmAmount) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kToneFreq) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kAmpAttack) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kAmpDecay) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kAmpSustain) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kAmpRelease) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kClickType) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kNoiseType) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kDistDrive) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kCompThresh) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kLimiterCeiling) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kReverbMix) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kKeyZonesEnabled) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kBodyVelocityDepth) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kWireAftertouchDepth) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kRollEnabled) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kRollProbability) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kMidiCcTransient) != nullptr);
}

TEST_CASE("OpenSmackerProcessor provides 72 factory presets organized into 24 SN, 24 BL, and 24 TM", "[plugin]") {
    OpenSmackerProcessor processor;
    REQUIRE(processor.getNumPrograms() == 72);

    for (int i = 0; i < 72; ++i) {
        processor.setCurrentProgram(i);
        REQUIRE(processor.getCurrentProgram() == i);
        const juce::String name = processor.getProgramName(i);
        REQUIRE(name.isNotEmpty());
        if (i < 24) {
            REQUIRE(name.startsWith("SN "));
        }
        if (i >= 24 && i < 48) {
            REQUIRE(name.startsWith("BL "));
        }
        if (i >= 48) {
            REQUIRE(name.startsWith("TM "));
        }
    }
}

TEST_CASE("OpenSmackerProcessor renders audio upon MIDI note-on", "[plugin]") {
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);

    processor.processBlock(buffer, midi);

    float maxSample = 0.0f;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            maxSample = std::max(maxSample, std::abs(buffer.getSample(ch, i)));
        }
    }

    REQUIRE(maxSample > 0.0f);
    REQUIRE(processor.displayPeak.load() > 0.0f);
    REQUIRE(processor.displayTriggerCount.load() >= 1);
}

TEST_CASE("OpenSmackerProcessor saves and restores state information", "[plugin]") {
    OpenSmackerProcessor proc1;
    proc1.setCurrentProgram(8);

    juce::MemoryBlock block;
    proc1.getStateInformation(block);
    REQUIRE(block.getSize() > 0);

    OpenSmackerProcessor proc2;
    proc2.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    const float p1 = proc1.parameters.getRawParameterValue(opensmacker::params::kDistDrive)->load();
    const float p2 = proc2.parameters.getRawParameterValue(opensmacker::params::kDistDrive)->load();
    REQUIRE(std::abs(p1 - p2) < 0.001f);
}

TEST_CASE("Section locks preserve preset values and restore with the session", "[plugin][preset_lock]") {
    OpenSmackerProcessor source;
    auto setValue = [&source](const char* id, float value) {
        auto* parameter = source.parameters.getParameter(id);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    source.setSectionLocked(opensmacker::presets::Section::VoiceTuning, true);
    setValue(opensmacker::params::kToneDecay, 777.0f);
    setValue(opensmacker::params::kNoiseLevel, 0.01f);
    source.setCurrentProgram(2);
    REQUIRE(std::abs(source.parameters.getRawParameterValue(opensmacker::params::kToneDecay)->load() - 777.0f) < 1.0f);
    REQUIRE(source.parameters.getRawParameterValue(opensmacker::params::kNoiseLevel)->load() > 0.4f);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    OpenSmackerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored.isSectionLocked(opensmacker::presets::Section::VoiceTuning));
    REQUIRE_FALSE(restored.isSectionLocked(opensmacker::presets::Section::WiresAir));
}

TEST_CASE("OpenSmackerEditor captures each page in both view modes", "[plugin][gui]") {
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    processor.setCurrentProgram(1);

    // Trigger a hit so the live visualizer stage is active in the screenshot
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)120), 0);
    processor.processBlock(buffer, midi);

    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() == 1100);
    REQUIRE(editor->getHeight() == 972);

    juce::File assetsDir = juce::File::getCurrentWorkingDirectory().getChildFile("assets");
    if (!assetsDir.exists()) {
        assetsDir = juce::File::getCurrentWorkingDirectory().getParentDirectory().getChildFile("assets");
    }
    REQUIRE(assetsDir.exists());
    const std::array<const char*, 4> pageNames{ "hit", "layers", "fx", "midi" };
    for (int page = 0; page < 4; ++page) {
        if (page < 3)
            processor.setCurrentProgram(page * 24 + 1);
        editor->setPage(page);
        for (int mode = 0; mode < 2; ++mode) {
            editor->setAdvancedView(mode == 1);
            const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
            REQUIRE(snapshot.isValid());
            REQUIRE(snapshot.getWidth() == 1100);
            REQUIRE(snapshot.getHeight() == 972);
            const juce::String suffix = mode == 0 ? "" : "-advanced";
            const juce::File screenshot = assetsDir.getChildFile(
                "openSmacker-" + juce::String(pageNames[static_cast<size_t>(page)]) + suffix + ".png");
            screenshot.deleteFile();
            juce::FileOutputStream stream(screenshot);
            REQUIRE(stream.openedOk());
            juce::PNGImageFormat png;
            REQUIRE(png.writeImageToStream(snapshot, stream));
        }
    }
}

TEST_CASE("Lock Pitch preserves note, octave, and fine pitch across preset changes", "[plugin][pitch_lock]") {
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto* lockParam = processor.parameters.getParameter(opensmacker::params::kLockPitch);
    REQUIRE(lockParam != nullptr);

    auto* octaveParam = processor.parameters.getParameter(opensmacker::params::kToneOctave);
    auto* freqParam = processor.parameters.getParameter(opensmacker::params::kToneFreq);
    auto* noteParam = processor.parameters.getParameter(opensmacker::params::kToneNote);
    auto* fineParam = processor.parameters.getParameter(opensmacker::params::kToneFine);
    REQUIRE(octaveParam != nullptr);
    REQUIRE(freqParam != nullptr);
    REQUIRE(noteParam != nullptr);
    REQUIRE(fineParam != nullptr);

    octaveParam->setValueNotifyingHost(octaveParam->convertTo0to1(6.0f));
    freqParam->setValueNotifyingHost(freqParam->convertTo0to1(2400.0f));
    noteParam->setValueNotifyingHost(noteParam->convertTo0to1(4.0f));
    fineParam->setValueNotifyingHost(fineParam->convertTo0to1(15.0f));

    lockParam->setValueNotifyingHost(1.0f);

    processor.setCurrentProgram(48);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneOctave)->load() - 6.0f) < 0.05f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneFreq)->load() - 2400.0f) < 2.0f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneNote)->load() - 4.0f) < 0.05f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneFine)->load() - 15.0f) < 0.5f);

    lockParam->setValueNotifyingHost(0.0f);
    processor.setCurrentProgram(48);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneOctave)->load() - 2.0f) < 0.05f);
}

TEST_CASE("All 72 factory presets render clean audio without NaNs or silences", "[plugin][presets]") {
    OpenSmackerProcessor processor;
    constexpr int renderSamples = 4096; // Covers the longest factory flam lead-in at 48 kHz.
    processor.prepareToPlay(48000.0, renderSamples);

    for (int p = 0; p < 72; ++p) {
        processor.setCurrentProgram(p);
        juce::AudioBuffer<float> buffer(2, renderSamples);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)110), 0);

        processor.processBlock(buffer, midi);

        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < renderSamples; ++i) {
                const float s = buffer.getSample(ch, i);
                REQUIRE_FALSE(std::isnan(s));
                REQUIRE_FALSE(std::isinf(s));
                peak = std::max(peak, std::abs(s));
            }
        }
        REQUIRE(peak > 0.05f);
        REQUIRE(peak <= 1.5f);
    }
}

TEST_CASE("OpenSmacker provides stereo component output buses", "[plugin][layers]") {
    OpenSmackerProcessor processor;
    REQUIRE(processor.getTotalNumOutputChannels() == 2);
    auto layout = processor.getBusesLayout();
    layout.outputBuses.set(1, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(2, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(3, juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(layout));
    REQUIRE(processor.getBusCount(false) == 4);
    REQUIRE(processor.getChannelCountOfBus(false, 0) == 2);
    REQUIRE(processor.getChannelCountOfBus(false, 1) == 2);
    REQUIRE(processor.getChannelCountOfBus(false, 2) == 2);
    REQUIRE(processor.getChannelCountOfBus(false, 3) == 2);
    processor.prepareToPlay(48000.0, 4096);
    juce::AudioBuffer<float> buffer(processor.getTotalNumOutputChannels(), 4096);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(110)), 0);
    processor.processBlock(buffer, midi);

    const auto energy = [&processor, &buffer](int bus) {
        auto output = processor.getBusBuffer(buffer, false, bus);
        float sum = 0.0f;
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
            sum += std::abs(output.getSample(0, sample)) + std::abs(output.getSample(1, sample));
        return sum;
    };
    REQUIRE(energy(0) > 0.1f);
    REQUIRE(energy(1) > 0.1f);
    REQUIRE(energy(2) > 0.1f);
    REQUIRE(energy(3) > 0.1f);
}

TEST_CASE("Parameter state maps voice zones roll timing and layer CC defaults", "[plugin][performance]") {
    OpenSmackerProcessor processor;
    const auto settings = opensmacker::params::readSettings(processor.parameters);
    REQUIRE_FALSE(settings.keyZonesEnabled);
    REQUIRE(settings.voiceZones[0].lowNote == 36);
    REQUIRE(settings.voiceZones[0].highNote == 47);
    REQUIRE(settings.voiceZones[1].lowNote == 48);
    REQUIRE(settings.voiceZones[3].highNote == 84);
    REQUIRE_FALSE(settings.rollEnabled);
    REQUIRE(settings.rollSubdivision == 16);
    REQUIRE(settings.rollAccentEvery == 4);
    REQUIRE(settings.midiCcForBody == 20);
    REQUIRE(settings.midiCcForWire == 21);
    REQUIRE(settings.midiCcForTransient == 22);
}

TEST_CASE("OpenSmacker routes MIDI channel pressure to the body layer", "[plugin][aftertouch]") {
    const auto renderBodyEnergy = [](bool withPressure) {
        OpenSmackerProcessor processor;
        auto layout = processor.getBusesLayout();
        layout.outputBuses.set(1, juce::AudioChannelSet::stereo());
        layout.outputBuses.set(2, juce::AudioChannelSet::stereo());
        layout.outputBuses.set(3, juce::AudioChannelSet::stereo());
        REQUIRE(processor.setBusesLayout(layout));
        auto* aftertouch = processor.parameters.getParameter(opensmacker::params::kBodyAftertouchDepth);
        REQUIRE(aftertouch != nullptr);
        aftertouch->setValueNotifyingHost(aftertouch->convertTo0to1(1.0f));
        processor.prepareToPlay(48000.0, 4096);
        juce::AudioBuffer<float> buffer(processor.getTotalNumOutputChannels(), 4096);
        juce::MidiBuffer midi;
        if (withPressure)
            midi.addEvent(juce::MidiMessage::channelPressureChange(1, 127), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(110)), 0);
        processor.processBlock(buffer, midi);

        auto body = processor.getBusBuffer(buffer, false, 1);
        float energy = 0.0f;
        for (int sample = 0; sample < body.getNumSamples(); ++sample)
            energy += std::abs(body.getSample(0, sample)) + std::abs(body.getSample(1, sample));
        return energy;
    };

    const float withoutPressure = renderBodyEnergy(false);
    const float withPressure = renderBodyEnergy(true);
    REQUIRE(withPressure > withoutPressure * 1.5f);
}

TEST_CASE("Lock-In state restores all voice zones and separated layer stems", "[plugin][lock_in][zones][layers]") {
    OpenSmackerProcessor original;
    auto* lockIn = original.parameters.getParameter(opensmacker::params::kLockIn);
    auto* zones = original.parameters.getParameter(opensmacker::params::kKeyZonesEnabled);
    REQUIRE(lockIn != nullptr);
    REQUIRE(zones != nullptr);
    lockIn->setValueNotifyingHost(1.0f);
    zones->setValueNotifyingHost(1.0f);
    original.prepareToPlay(48000.0, 1024);
    juce::AudioBuffer<float> warmup(2, 1024);
    juce::MidiBuffer noMidi;
    original.processBlock(warmup, noMidi);
    REQUIRE(original.isSmackLockedIn());

    juce::MemoryBlock state;
    original.getStateInformation(state);
    REQUIRE(state.getSize() > 1000);

    OpenSmackerProcessor restored;
    auto layout = restored.getBusesLayout();
    layout.outputBuses.set(1, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(2, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(3, juce::AudioChannelSet::stereo());
    REQUIRE(restored.setBusesLayout(layout));
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored.isSmackLockedIn());
    restored.prepareToPlay(48000.0, 2048);

    const auto renderNote = [&restored](int note) {
        juce::AudioBuffer<float> buffer(restored.getTotalNumOutputChannels(), 2048);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(110)), 0);
        restored.processBlock(buffer, midi);
        auto main = restored.getBusBuffer(buffer, false, 0);
        auto body = restored.getBusBuffer(buffer, false, 1);
        auto wire = restored.getBusBuffer(buffer, false, 2);
        auto transient = restored.getBusBuffer(buffer, false, 3);
        std::array<float, 4> result{};
        for (int sample = 0; sample < main.getNumSamples(); ++sample) {
            result[0] += std::abs(main.getSample(0, sample));
            result[1] += std::abs(body.getSample(0, sample));
            result[2] += std::abs(wire.getSample(0, sample));
            result[3] += std::abs(transient.getSample(0, sample));
        }
        return result;
    };

    const auto snare = renderNote(36);
    const auto bell = renderNote(60);
    REQUIRE(snare[0] > 0.1f);
    REQUIRE(bell[0] > 0.1f);
    REQUIRE(std::abs(snare[0] - bell[0]) > 0.01f);
    for (size_t layer = 1; layer < snare.size(); ++layer) {
        REQUIRE(snare[layer] > 0.1f);
        REQUIRE(bell[layer] > 0.1f);
    }
}

