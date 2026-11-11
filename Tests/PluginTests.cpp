#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include <catch2/catch_test_macros.hpp>
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
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kClickType) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kNoiseType) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kDistDrive) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kCompThresh) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kLimiterCeiling) != nullptr);
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kReverbMix) != nullptr);
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
    proc1.setCurrentProgram(8); // SN 09 Euphoric Always Alive Snare

    juce::MemoryBlock block;
    proc1.getStateInformation(block);
    REQUIRE(block.getSize() > 0);

    OpenSmackerProcessor proc2;
    proc2.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    const float p1 = proc1.parameters.getRawParameterValue(opensmacker::params::kDistDrive)->load();
    const float p2 = proc2.parameters.getRawParameterValue(opensmacker::params::kDistDrive)->load();
    REQUIRE(std::abs(p1 - p2) < 0.001f);
}

TEST_CASE("OpenSmackerEditor instantiates and captures live Simple and Advanced screenshots", "[plugin][gui]") {
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    processor.setCurrentProgram(1); // SN 02 Melodic Group Therapy Snare

    // Trigger a hit so the live visualizer stage is active in the screenshot
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)120), 0);
    processor.processBlock(buffer, midi);

    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() == 1100);
    REQUIRE(editor->getHeight() == 740);

    // 1. Capture Simple Mode Screenshot
    juce::Image simpleSnapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    REQUIRE(simpleSnapshot.isValid());
    REQUIRE(simpleSnapshot.getWidth() == 1100);
    REQUIRE(simpleSnapshot.getHeight() == 740);

    juce::File assetsDir = juce::File::getCurrentWorkingDirectory().getChildFile("assets");
    if (!assetsDir.exists()) {
        assetsDir = juce::File::getCurrentWorkingDirectory().getParentDirectory().getChildFile("assets");
    }
    juce::File repoScreenshot = assetsDir.getChildFile("openSmacker-screenshot.png");
    if (repoScreenshot.getParentDirectory().exists()) {
        repoScreenshot.deleteFile();
        juce::FileOutputStream stream(repoScreenshot);
        if (stream.openedOk()) {
            juce::PNGImageFormat png;
            png.writeImageToStream(simpleSnapshot, stream);
        }
    }

    // 2. Capture Advanced Mode Screenshot (showing BL 04 Euphoric 138 Pluck Bell)
    processor.setCurrentProgram(27); // BL 04 Euphoric 138 Pluck Bell
    editor->setAdvancedView(true);
    juce::Image advSnapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    REQUIRE(advSnapshot.isValid());

    juce::File advScreenshot = assetsDir.getChildFile("openSmacker-advanced-screenshot.png");
    if (advScreenshot.getParentDirectory().exists()) {
        advScreenshot.deleteFile();
        juce::FileOutputStream stream(advScreenshot);
        if (stream.openedOk()) {
            juce::PNGImageFormat png;
            png.writeImageToStream(advSnapshot, stream);
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

    processor.setCurrentProgram(48); // TM 01 Progressive Groove Tom has octave 2 default
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneOctave)->load() - 6.0f) < 0.05f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneFreq)->load() - 2400.0f) < 2.0f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneNote)->load() - 4.0f) < 0.05f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneFine)->load() - 15.0f) < 0.5f);

    lockParam->setValueNotifyingHost(0.0f);
    processor.setCurrentProgram(48);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneOctave)->load() - 2.0f) < 0.05f);
}
