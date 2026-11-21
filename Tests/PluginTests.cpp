#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "Presets.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

namespace {
template<class T, class Predicate>
T* findVisible(juce::Component& parent, const Predicate& matches) {
    for (auto* child : parent.getChildren()) {
        if (!child->isVisible()) continue;
        if (auto* component = dynamic_cast<T*>(child); component && matches(*component)) return component;
        if (auto* found = findVisible<T>(*child, matches)) return found;
    }
    return nullptr;
}
void checkVisibleButtonTargets(juce::Component& component, juce::Component& editor,
                               std::vector<juce::Rectangle<int>>& targets) {
    for (auto* child : component.getChildren()) {
        if (!child->isVisible()) continue;
        const auto rect = editor.getLocalArea(child, child->getLocalBounds());
        CAPTURE(child->getName().toStdString(), rect.toString().toStdString());
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            INFO(button->getButtonText());
            const juce::Font font(juce::FontOptions(14.0f, juce::Font::bold));
            CHECK(juce::GlyphArrangement::getStringWidthInt(font, button->getButtonText()) + 8 <= rect.getWidth());
            CHECK(button->getLookAndFeel().getTextButtonFont(*button, button->getHeight()).getHeight() >= 14.0f);
        } else if (auto* box = dynamic_cast<juce::ComboBox*>(child)) {
            const auto font = box->getLookAndFeel().getComboBoxFont(*box);
            for (int i = 0; i < box->getNumItems(); ++i) {
                INFO(box->getItemText(i));
                CHECK(juce::GlyphArrangement::getStringWidthInt(font, box->getItemText(i)) + 38 <= rect.getWidth());
            }
        } else if (auto* slider = dynamic_cast<juce::Slider*>(child);
                   slider != nullptr && slider->isRotary() && slider->getTextBoxPosition() != juce::Slider::NoTextBox) {
            const auto dial = slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds;
            CHECK(std::min(dial.getWidth(), dial.getHeight()) >= 36);
        } else if (!dynamic_cast<juce::Slider*>(child)) {
            checkVisibleButtonTargets(*child, editor, targets);
            continue;
        }
        CHECK(rect.getHeight() >= 36);
        CHECK(rect.getWidth() >= 44);
        CHECK(editor.getLocalBounds().contains(rect));
        CHECK(std::none_of(targets.begin(), targets.end(), [rect](auto other) { return rect.intersects(other); }));
        targets.push_back(rect);
    }
}
}

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
    REQUIRE(processor.parameters.getParameter(opensmacker::params::kAmpEnabled) != nullptr);
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
    for (const auto* id : { opensmacker::params::kBodyHPF, opensmacker::params::kBodyLPF,
                            opensmacker::params::kBodyPan, opensmacker::params::kWireTension,
                            opensmacker::params::kWirePan, opensmacker::params::kTransientHPF,
                            opensmacker::params::kTransientLPF, opensmacker::params::kTransientPan,
                            opensmacker::params::kShellSpread, opensmacker::params::kSubThump,
                            opensmacker::params::kEchoMix }) {
        auto* parameter = processor.parameters.getParameter(id);
        REQUIRE(parameter != nullptr);
        CHECK(parameter->isAutomatable());
    }
}

TEST_CASE("OpenSmacker layer controls map to engine settings and survive state restore", "[plugin][parameters][state]") {
    OpenSmackerProcessor source;
    const std::array<std::pair<const char*, float>, 8> values {{
        { opensmacker::params::kBodyHPF, 420.0f },
        { opensmacker::params::kBodyLPF, 6700.0f },
        { opensmacker::params::kBodyPan, -0.6f },
        { opensmacker::params::kWireTension, 0.9f },
        { opensmacker::params::kWirePan, 0.35f },
        { opensmacker::params::kTransientHPF, 1500.0f },
        { opensmacker::params::kTransientLPF, 9800.0f },
        { opensmacker::params::kTransientPan, 0.75f }
    }};
    for (const auto& [id, value] : values) {
        auto* parameter = source.parameters.getParameter(id);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    const auto settings = opensmacker::params::readSettings(source.parameters);
    CHECK(std::abs(settings.synth.bodyHPF - 420.0f) < 1.1f);
    CHECK(std::abs(settings.synth.bodyPan + 0.6f) < 0.011f);
    CHECK(std::abs(settings.synth.wireTension - 0.9f) < 0.011f);
    CHECK(std::abs(settings.synth.transientLPF - 9800.0f) < 1.1f);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    OpenSmackerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    for (const auto& [id, value] : values)
        CHECK(std::abs(restored.parameters.getRawParameterValue(id)->load() - value) < 1.1f);
}

TEST_CASE("Every float parameter reads back the text it displays", "[plugin][parameters]") {
    OpenSmackerProcessor processor;
    for (auto* parameter : processor.getParameters()) {
        const auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);
        if (floatParameter == nullptr)
            continue;
        for (int step = 0; step <= 40; ++step) {
            const auto text = parameter->getText(static_cast<float>(step) / 40.0f, 0);
            CAPTURE(floatParameter->paramID.toStdString(), text.toStdString());
            CHECK(parameter->getText(parameter->getValueForText(text), 0) == text);
        }
    }
}

TEST_CASE("Displays use one unit format and typed values accept those units", "[plugin][parameters]") {
    using namespace opensmacker::params;
    OpenSmackerProcessor processor;
    const auto shown = [&processor](const char* id, float value) {
        auto* parameter = processor.parameters.getParameter(id);
        return parameter->getText(parameter->convertTo0to1(value), 0);
    };
    const auto typed = [&processor](const char* id, const char* text) {
        auto* parameter = processor.parameters.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValueForText(text));
    };
    CHECK(shown(kDistMix, 0.55f) == "55%");
    CHECK(shown(kDistTone, 0.2f) == "+20%");
    CHECK(shown(kCompMakeup, 3.0f) == "+3.0 dB");
    CHECK(shown(kEqLowDb, -0.04f) == "0.0 dB");
    CHECK(shown(kBodyLPF, 850.0f) == "850 Hz");
    CHECK(shown(kNoiseLPF, 6800.0f) == "6.80 kHz");
    CHECK(shown(kClickFreq, 12000.0f) == "12.0 kHz");
    CHECK(shown(kAmpAttack, 4.5f) == "4.5 ms");
    CHECK(shown(kToneDecay, 160.0f) == "160 ms");
    CHECK(shown(kAmpRelease, 1200.0f) == "1.20 s");
    CHECK(shown(kCompRatio, 2.5f) == "2.5:1");
    CHECK(shown(kBodyPan, 0.0f) == "C");
    CHECK(shown(kBodyPan, -0.4f) == "40 L");
    CHECK(shown(kWirePan, 0.4f) == "40 R");

    CHECK(typed(kClickFreq, "3.2k") == Catch::Approx(3200.0f));
    CHECK(typed(kClickFreq, "3.2 kHz") == Catch::Approx(3200.0f));
    CHECK(typed(kReverbMix, "50") == Catch::Approx(0.5f));
    CHECK(typed(kReverbMix, "50%") == Catch::Approx(0.5f));
    CHECK(typed(kAmpRelease, "1.2 s") == Catch::Approx(1200.0f));
    CHECK(typed(kAmpRelease, "160 ms") == Catch::Approx(160.0f));
    CHECK(typed(kOutputGain, "-3 dB") == Catch::Approx(-3.0f));
    CHECK(typed(kBodyPan, "40 L") == Catch::Approx(-0.4f));
    CHECK(typed(kBodyPan, "C") == Catch::Approx(0.0f).margin(1.0e-6));
    CHECK(typed(kCompRatio, "2.5:1") == Catch::Approx(2.5f));
}

TEST_CASE("The factory bank has 84 distinct programs across six dance styles and four voices", "[plugin][presets]") {
    OpenSmackerProcessor processor;
    std::set<std::string> names;
    REQUIRE(processor.getNumPrograms() == 84);
    std::array<int, 4> counts{};
    const std::array<int, 4> modes{ 0, 2, 1, 3 };
    for (int i = 0; i < processor.getNumPrograms(); ++i) {
        processor.setCurrentProgram(i);
        REQUIRE(processor.getCurrentProgram() == i);
        CHECK(names.insert(processor.getProgramName(i).toStdString()).second);
        const auto family = opensmacker::presets::familyForProgram(i);
        ++counts[family];
        CHECK(static_cast<int>(processor.currentSettings().synth.voiceMode) == modes[family]);
        CHECK_FALSE(processor.currentSettings().synth.fullAdsrEnabled);
    }
    for (int count : counts)
        CHECK(count == 21);
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

TEST_CASE("OpenSmackerProcessor routes note-off into the optional Full ADSR tail", "[plugin][adsr][midi]") {
    OpenSmackerProcessor processor;
    auto* fullAdsr = processor.parameters.getParameter(opensmacker::params::kAmpEnabled);
    auto* release = processor.parameters.getParameter(opensmacker::params::kAmpRelease);
    REQUIRE(fullAdsr != nullptr);
    REQUIRE(release != nullptr);
    fullAdsr->setValueNotifyingHost(1.0f);
    release->setValueNotifyingHost(release->convertTo0to1(20000.0f));
    processor.prepareToPlay(8000.0, 2048);

    juce::AudioBuffer<float> buffer(2, 2048);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(110)), 0);
    midi.addEvent(juce::MidiMessage::noteOff(2, 60), 256);
    processor.processBlock(buffer, midi);

    float beforeNoteOff = 0.0f;
    float afterNoteOff = 0.0f;
    for (int sample = 0; sample < 256; ++sample)
        beforeNoteOff += std::abs(buffer.getSample(0, sample));
    for (int sample = 512; sample < buffer.getNumSamples(); ++sample)
        afterNoteOff += std::abs(buffer.getSample(0, sample));
    REQUIRE(beforeNoteOff > 0.1f);
    REQUIRE(afterNoteOff > 0.1f);
    REQUIRE(processor.getTailLengthSeconds() >= 20.0);
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

TEST_CASE("Each card's Lock keeps only that card, and presets leave MIDI pitch alone", "[plugin][preset_lock]") {
    using namespace opensmacker::params;
    using opensmacker::presets::Section;
    OpenSmackerProcessor processor;
    const auto set = [&processor](const char* id, float value) {
        auto* parameter = processor.parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    const auto value = [&processor](const char* id) { return processor.parameters.getRawParameterValue(id)->load(); };
    set(kLimiterCeiling, -6.0f);
    set(kLimiterRelease, 80.0f);
    set(kLimiterEnabled, 0.0f);
    set(kAmpAttack, 15.0f);
    set(kMidiMode, 1.0f);
    processor.setSectionLocked(Section::Output, true);
    processor.setSectionLocked(Section::VoiceTuning, true);

    processor.setCurrentProgram(5);

    CHECK(value(kLimiterCeiling) == Catch::Approx(-6.0f));
    CHECK(value(kLimiterRelease) == Catch::Approx(80.0f));
    CHECK(value(kLimiterEnabled) < 0.5f);
    CHECK(value(kAmpAttack) == Catch::Approx(0.2f));
    CHECK(value(kMidiMode) > 0.5f);
}

TEST_CASE("Sessions from before per-card locks keep the locks their cards showed", "[plugin][preset_lock][state]") {
    using opensmacker::presets::Section;
    OpenSmackerProcessor source;
    auto tree = source.parameters.copyState();
    tree.setProperty("factoryBank", 2, nullptr);
    tree.setProperty("section_lock_0", true, nullptr);
    tree.setProperty("section_lock_5", true, nullptr);
    juce::MemoryBlock oldSession;
    juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), oldSession);
    OpenSmackerProcessor restored;
    restored.setStateInformation(oldSession.getData(), static_cast<int>(oldSession.getSize()));
    CHECK(restored.isSectionLocked(Section::Envelope));
    CHECK(restored.isSectionLocked(Section::BodyDetail));
    CHECK(restored.isSectionLocked(Section::Output));
    CHECK(restored.isSectionLocked(Section::Space));
    CHECK_FALSE(restored.isSectionLocked(Section::SaturationDynamics));

    OpenSmackerProcessor modern;
    modern.setSectionLocked(Section::VoiceTuning, true);
    juce::MemoryBlock session;
    modern.getStateInformation(session);
    OpenSmackerProcessor reopened;
    reopened.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    CHECK(reopened.isSectionLocked(Section::VoiceTuning));
    CHECK_FALSE(reopened.isSectionLocked(Section::Envelope));
    CHECK_FALSE(reopened.isSectionLocked(Section::BodyDetail));
}

TEST_CASE("The MIDI page shows note names, Off and steppers, and Learn sets a CC slot", "[plugin][gui][midi]") {
    using namespace opensmacker::params;
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    auto* snareLow = processor.parameters.getParameter(kSnareZoneLow);
    CHECK(snareLow->getCurrentValueAsText() == "C2");
    CHECK(snareLow->convertFrom0to1(snareLow->getValueForText("C#4")) == 61.0f);
    CHECK(snareLow->convertFrom0to1(snareLow->getValueForText("Db2")) == 37.0f);
    CHECK(snareLow->convertFrom0to1(snareLow->getValueForText("40")) == 40.0f);
    CHECK(processor.parameters.getParameter(kRollNote)->getCurrentValueAsText() == "A4");
    auto* bodyCc = processor.parameters.getParameter(kMidiCcBody);
    bodyCc->setValueNotifyingHost(0.0f);
    CHECK(bodyCc->getCurrentValueAsText() == "Off");
    CHECK(bodyCc->getValueForText("Off") == 0.0f);

    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    const auto slider = [&editor](juce::String name) {
        return findVisible<juce::Slider>(*editor, [name](const auto& s) { return s.getName() == name; });
    };
    const auto button = [&editor](juce::String text) {
        return findVisible<juce::TextButton>(*editor, [text](const auto& b) { return b.getButtonText() == text; });
    };
    button("Advanced controls")->onClick();
    REQUIRE(slider("Octave") != nullptr);
    CHECK(slider("Octave")->getSliderStyle() == juce::Slider::IncDecButtons);
    button("Layers")->onClick();
    REQUIRE(slider("Hits") != nullptr);
    CHECK(slider("Hits")->getSliderStyle() == juce::Slider::IncDecButtons);
    button("MIDI")->onClick();
    REQUIRE(slider("Accent every") != nullptr);
    CHECK(slider("Accent every")->getSliderStyle() == juce::Slider::IncDecButtons);
    CHECK(slider("Snare low")->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag);

    auto* learn = findVisible<juce::TextButton>(*editor, [](const auto& b) { return b.getTitle() == "Learn Body CC"; });
    REQUIRE(learn != nullptr);
    learn->onClick();
    CHECK(learn->getButtonText() == "Listening");
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 33, 90), 0);
    processor.processBlock(buffer, midi);
    auto* learnTimer = dynamic_cast<juce::Timer*>(learn->getParentComponent());
    REQUIRE(learnTimer != nullptr);
    learnTimer->timerCallback();
    CHECK(processor.currentSettings().midiCcForBody == 33);
    CHECK(learn->getButtonText() == "Learn");
}

TEST_CASE("Every slider, menu and button has a spoken title", "[plugin][gui][accessibility]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    juce::StringArray untitled;
    int titled = 0;
    std::function<void(juce::Component&)> visit = [&](juce::Component& component) {
        const bool control = dynamic_cast<juce::Slider*>(&component) != nullptr || dynamic_cast<juce::ComboBox*>(&component) != nullptr
            || dynamic_cast<juce::Button*>(&component) != nullptr;
        if (control && component.isAccessible() && component.getTitle().isEmpty())
            untitled.add(component.getName());
        if (control && !component.getTitle().isEmpty())
            ++titled;
        std::ranges::for_each(component.getChildren(), [&visit](juce::Component* child) { visit(*child); });
    };
    visit(*editor);
    CAPTURE(untitled.joinIntoString(", ").toStdString());
    CHECK(untitled.isEmpty());
    CHECK(titled > 100);
}

TEST_CASE("OpenSmackerEditor captures each page in both view modes", "[plugin][gui]") {
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    processor.setCurrentProgram(0);

    // Exercise the real processor before opening the editor.
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)120), 0);
    processor.processBlock(buffer, midi);

    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() == 1120);
    REQUIRE(editor->getHeight() == 752);

    auto click = [&editor](juce::String name) {
        auto* button = findVisible<juce::TextButton>(*editor, [name](const auto& b) { return b.getButtonText() == name; });
        REQUIRE(button != nullptr);
        button->onClick();
    };
    auto* gate = findVisible<juce::ComboBox>(*editor, [](const auto& c) { return c.getName() == "MIDI gate"; });
    REQUIRE(gate != nullptr);
    gate->setSelectedId(2, juce::sendNotificationSync);
    REQUIRE(processor.currentSettings().synth.fullAdsrEnabled);
    for (auto name : { "Attack", "Decay", "Sustain", "Release" }) {
        auto* slider = findVisible<juce::Slider>(*editor, [name](auto& c) { return c.getName() == name; });
        REQUIRE(slider != nullptr);
        if (juce::String(name) == "Release") {
            slider->setValue(20000, juce::sendNotificationSync);
            CHECK(processor.currentSettings().synth.ampReleaseMs == 20000.0f);
        }
    }
    click("Audition hit");
    midi.clear();
    processor.processBlock(buffer, midi);
    REQUIRE(processor.displayTriggerCount == 2);
    for (int block = 0; block < 70; ++block) processor.processBlock(buffer, midi);
    CHECK(buffer.getMagnitude(0, 512) < 0.0001f);
    click("MIDI");
    auto* pitch = findVisible<juce::ComboBox>(*editor, [](auto& c) { return c.getName() == "MIDI pitch"; });
    REQUIRE(pitch != nullptr);
    pitch->setSelectedId(2, juce::sendNotificationSync);
    CHECK(processor.currentSettings().noteTracking);
    click("Hit");
    (void) editor->createComponentSnapshot(editor->getLocalBounds());
    click("Lock");
    REQUIRE(processor.isSectionLocked(opensmacker::presets::Section::VoiceTuning));
    (void) editor->createComponentSnapshot(editor->getLocalBounds());
    click("Locked");
    REQUIRE_FALSE(processor.isSectionLocked(opensmacker::presets::Section::VoiceTuning));
    const int program = processor.getCurrentProgram();
    auto* voice = processor.parameters.getParameter(opensmacker::params::kVoiceMode);
    auto railButton = [&editor](juce::String name) {
        return findVisible<juce::TextButton>(*editor, [name](const auto& b) { return b.getButtonText() == name; });
    };
    click("Rim");
    CHECK(voice->getCurrentValueAsText() == "Rim");
    CHECK(processor.getCurrentProgram() == program);
    CHECK(railButton("Rim")->getToggleState());
    auto* voiceMenu = findVisible<juce::ComboBox>(*editor, [](auto& c) { return c.getName() == "Voice"; });
    REQUIRE(voiceMenu != nullptr);
    voiceMenu->setSelectedId(2, juce::sendNotificationSync);
    CHECK(railButton("Tom")->getToggleState());
    CHECK_FALSE(railButton("Rim")->getToggleState());
    click("Next");
    CHECK(processor.getCurrentProgram() == program + 1);
    click("Previous");
    CHECK(processor.getCurrentProgram() == program);
    click("Snare");
    CHECK(voice->getCurrentValueAsText() == "Snare");
    auto* browser = findVisible<juce::ComboBox>(*editor, [](auto& c) { return c.getName() == "Preset"; });
    REQUIRE(browser != nullptr);
    CHECK(browser->getRootMenu()->getNumItems() == 6);
    browser->setSelectedId(2, juce::sendNotificationSync);
    CHECK(processor.getCurrentProgram() == 1);
    browser->setSelectedId(1, juce::sendNotificationSync);
    const std::array<const char*, 4> pages{ "Hit", "Layers", "Effects", "MIDI" };

    juce::File assetsDir;
#if defined(OPENSMACKER_SOURCE_DIR)
    assetsDir = juce::File(OPENSMACKER_SOURCE_DIR).getChildFile("assets");
#endif
    if (!assetsDir.exists())
        assetsDir = juce::File::getCurrentWorkingDirectory().getChildFile("assets");
    if (!assetsDir.exists()) {
        assetsDir = juce::File::getCurrentWorkingDirectory().getParentDirectory().getChildFile("assets");
    }
    REQUIRE(assetsDir.exists());
    const std::array<const char*, 4> pageNames{ "hit", "layers", "fx", "midi" };
    for (int page = 0; page < 4; ++page) {
        click(pages[static_cast<size_t>(page)]);
        for (int mode = 0; mode < 2; ++mode) {
            if (mode == 1) click("Advanced controls");
            if (page == 0) gate->setSelectedId(mode + 1, juce::sendNotificationSync);
            std::vector<juce::Rectangle<int>> targets;
            checkVisibleButtonTargets(*editor, *editor, targets);
            const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
            REQUIRE(snapshot.isValid());
            REQUIRE(snapshot.getWidth() == 1120);
            REQUIRE(snapshot.getHeight() == 752);
            const juce::String suffix = mode == 0 ? "" : "-advanced";
            const juce::File screenshot = assetsDir.getChildFile(
                "openSmacker-" + juce::String(pageNames[static_cast<size_t>(page)]) + suffix + ".png");
            screenshot.deleteFile();
            juce::FileOutputStream stream(screenshot);
            REQUIRE(stream.openedOk());
            juce::PNGImageFormat png;
            REQUIRE(png.writeImageToStream(snapshot, stream));
        }
        click("Advanced controls");
    }
}

TEST_CASE("The Presets button opens a sized browser that points at the button", "[plugin][gui][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("opensmacker-launch-presets");
    root.deleteRecursively();
    OpenSmackerProcessor processor;
    processor.setPresetDocumentsRoot(root);
    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    juce::Component host;
    host.setBounds(0, 0, 1600, 1100);
    host.addAndMakeVisible(*editor);
    editor->setTopLeftPosition(300, 200);
    auto* presets = findVisible<juce::TextButton>(*editor, [](const auto& b) { return b.getButtonText() == "Presets"; });
    REQUIRE(presets != nullptr);

    presets->onClick();

    auto* callout = findVisible<juce::CallOutBox>(*editor, [](const auto&) { return true; });
    REQUIRE(callout != nullptr);
    auto* browser = findVisible<suite::PresetBrowser>(*callout, [](const auto&) { return true; });
    REQUIRE(browser != nullptr);
    CHECK(browser->getWidth() >= 300);
    CHECK(browser->getHeight() >= 400);
    CHECK(browser->visiblePresetCount() == processor.getNumPrograms());
    auto* save = findVisible<juce::TextButton>(*browser, [](const auto& b) { return b.getButtonText() == "Save"; });
    REQUIRE(save != nullptr);
    CHECK(save->getHeight() >= 28);
    const auto calloutArea = editor->getLocalArea(callout, callout->getLocalBounds());
    const auto buttonArea = editor->getLocalArea(presets, presets->getLocalBounds());
    CHECK(editor->getLocalBounds().contains(calloutArea));
    CHECK(std::abs(calloutArea.getY() - buttonArea.getBottom()) <= 12);
    CHECK(calloutArea.getX() < buttonArea.getRight());
    CHECK(calloutArea.getRight() > buttonArea.getX());
    host.removeChildComponent(editor.get());
    root.deleteRecursively();
}

TEST_CASE("OpenSmacker exposes 3-band EQ with glide and Output controls on the main Hit page", "[plugin][ui][eq]") {
    opensmacker::core::ThreeBandEq eqBoost, eqCut;
    eqBoost.prepare(48000.0);
    eqCut.prepare(48000.0);
    const opensmacker::core::ThreeBandEqSettings boost{ 80.0f, 0.0f, 1000.0f, 12.0f, 7500.0f, 0.0f };
    const opensmacker::core::ThreeBandEqSettings cut{ 80.0f, 0.0f, 1000.0f, -12.0f, 7500.0f, 0.0f };
    double rmsBoost = 0.0;
    double rmsCut = 0.0;
    std::ranges::for_each(std::views::iota(0, 4800), [&](int i) {
        const float x = 0.25f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / 48000.0f);
        float bL = x;
        float bR = x;
        eqBoost.process(bL, bR, boost);
        float cL = x;
        float cR = x;
        eqCut.process(cL, cR, cut);
        if (i >= 2400) {
            rmsBoost += static_cast<double>(bL) * bL;
            rmsCut += static_cast<double>(cL) * cL;
        }
    });
    REQUIRE(10.0 * std::log10(rmsBoost / rmsCut) > 20.0);

    OpenSmackerProcessor processor;
    std::ranges::for_each(std::array<const char*, 6>{
        "eq_low_freq", "eq_low_db", "eq_mid_freq", "eq_mid_db", "eq_high_freq", "eq_high_db"},
        [&](const char* id) { REQUIRE(processor.parameters.getParameter(id) != nullptr); });

    std::unique_ptr<OpenSmackerEditor> editor(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);
    std::ranges::for_each(std::array<const char*, 7>{
        "Low EQ gain", "Mid EQ gain", "High EQ gain", "Stereo width", "Output gain", "Limiter ceiling", "Limiter release"},
        [&](const char* title) {
            auto* slider = findVisible<juce::Slider>(*editor, [&](const auto& s) { return s.getTitle() == title; });
            REQUIRE(slider != nullptr);
            CHECK(slider->isVisible());
        });
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

    const int tomPreset = opensmacker::presets::firstPresetForFamily(2);
    processor.setCurrentProgram(tomPreset);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneOctave)->load() - 6.0f) < 0.05f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneFreq)->load() - 2400.0f) < 2.0f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneNote)->load() - 4.0f) < 0.05f);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneFine)->load() - 15.0f) < 0.5f);

    lockParam->setValueNotifyingHost(0.0f);
    processor.setCurrentProgram(tomPreset);
    REQUIRE(std::abs(processor.parameters.getRawParameterValue(opensmacker::params::kToneOctave)->load() - 2.0f) < 0.05f);
}

TEST_CASE("All factory hits have attack, body, headroom and a compact tail", "[plugin][audio][presets]") {
    constexpr int rate = 48000, samples = rate / 2;
    OpenSmackerProcessor processor;
    for (int family = 0; family < 4; ++family) {
        float lowPeak = 1, highPeak = 0;
        int shortest = 500, longest = 0;
        for (int index = 0; index < processor.getNumPrograms(); ++index) {
            if (static_cast<int>(opensmacker::presets::familyForProgram(index)) != family) continue;
            processor.setCurrentProgram(index);
            CAPTURE(processor.getProgramName(index).toStdString());
            processor.prepareToPlay(rate, samples);
            juce::AudioBuffer<float> buffer(2, samples);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(110)), 0);
            processor.processBlock(buffer, midi);
            const float peak = buffer.getMagnitude(0, samples);
            REQUIRE(peak > 0.25f);
            REQUIRE(peak <= 1.0f);
            CHECK(buffer.getMagnitude(0, 960) > peak * 0.2f);
            CHECK(buffer.getRMSLevel(0, 960, 960) > 0.025f);
            const auto cutoff = peak * 0.01f;
            int end = 0;
            for (int i = 0; i < samples; ++i) {
                const float l = buffer.getSample(0, i), r = buffer.getSample(1, i);
                REQUIRE((std::isfinite(l) && std::isfinite(r)));
                if (std::max(std::abs(l), std::abs(r)) >= cutoff) end = i;
            }
            const auto endMs = end * 1000 / rate;
            CHECK(endMs < 280);
            lowPeak = std::min(lowPeak, peak);
            highPeak = std::max(highPeak, peak);
            shortest = std::min(shortest, endMs);
            longest = std::max(longest, endMs);
        }
        std::cout << processor.parameters.getParameter(opensmacker::params::kVoiceMode)->getCurrentValueAsText()
                  << " peak " << lowPeak << " to " << highPeak << ", -40 dB tail "
                  << shortest << " to " << longest << " ms\n";
    }
}

TEST_CASE("Earlier sessions keep their exact voice settings and original resonances", "[plugin][state][presets]") {
    OpenSmackerProcessor source, restored;
    auto* hold = source.parameters.getParameter(opensmacker::params::kAmpEnabled);
    hold->setValueNotifyingHost(1);
    auto tree = source.parameters.copyState();
    tree.removeProperty("presetName", nullptr);
    auto xml = tree.createXml();
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored.presetName().isEmpty());
    auto expected = source.currentSettings().synth;
    expected.articulatedModes = false;
    expected.shapedEngine = false;
    REQUIRE(restored.currentSettings().synth == expected);
    restored.getStateInformation(state);
    OpenSmackerProcessor reopened;
    reopened.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(reopened.currentSettings().synth == expected);
    reopened.setCurrentProgram(0);
    CHECK(reopened.currentSettings().synth.articulatedModes);
    CHECK(reopened.currentSettings().synth.shapedEngine);
    CHECK_FALSE(reopened.currentSettings().synth.fullAdsrEnabled);
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
    REQUIRE_FALSE(settings.synth.fullAdsrEnabled);
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
    for (int callback = 0; callback < 2000 && !original.isSmackLockedIn(); ++callback)
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



TEST_CASE("Drumhead animation follows transport tempo and seeks and rests when stopped", "[plugin][gui_clock]") {
    juce::ScopedJuceInitialiser_GUI gui;
    struct Host : juce::AudioPlayHead {
        PositionInfo position;
        juce::Optional<PositionInfo> getPosition() const override { return position; }
    } host;
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000, 480);
    host.position.setBpm(120);
    host.position.setIsPlaying(true);
    host.position.setPpqPosition(3.25);
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> audio(2, 480);
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
    CHECK(std::abs(processor.animationBeatPhase(0) - 0.27f) < 0.001f);
    host.position.setPpqPosition(-1.5);
    host.position.setBpm(60);
    processor.processBlock(audio, midi);
    CHECK(std::abs(processor.animationBeatPhase(0) - 0.51f) < 0.001f);
    host.position.setIsPlaying(false);
    processor.processBlock(audio, midi);
    CHECK(processor.animationBeatPhase(0) == 1.0f);
    CHECK(processor.animationBeatPhase(20) == 1.0f);
    auto editor = std::unique_ptr<OpenSmackerEditor>(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    const auto drumhead = editor->drumheadBounds();
    auto resting = editor->createComponentSnapshot(drumhead);
    host.position.setIsPlaying(true);
    host.position.setPpqPosition(0.0);
    processor.processBlock(audio, midi);
    auto struck = editor->createComponentSnapshot(drumhead);
    int changedPixels = 0;
    for (int y = 0; y < struck.getHeight(); ++y)
        for (int x = 0; x < struck.getWidth(); ++x)
            if (struck.getPixelAt(x, y) != resting.getPixelAt(x, y)) ++changedPixels;
    CHECK(changedPixels > 200);
    processor.setPlayHead(nullptr);
    processor.processBlock(audio, midi);
    CHECK(std::abs(processor.animationBeatPhase(2.75) - 0.75f) < 0.001f);
}

TEST_CASE("OpenSmacker shell spread, sub thump, and tempo echo alter output", "[plugin][dsp]") {
    auto renderEnergy = [](float shellSpread, float subThump, float echoMix) {
        OpenSmackerProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto setP = [&](const char* id, float val) {
            auto* p = proc.parameters.getParameter(id);
            REQUIRE(p != nullptr);
            p->setValueNotifyingHost(p->convertTo0to1(val));
        };
        setP(opensmacker::params::kShellSpread, shellSpread);
        setP(opensmacker::params::kSubThump, subThump);
        setP(opensmacker::params::kEchoMix, echoMix);
        juce::AudioBuffer<float> buffer(8, 24000);
        buffer.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(110)), 0);
        proc.processBlock(buffer, midi);
        double early = 0.0, late = 0.0;
        for (int i = 0; i < 4000; ++i)
            early += std::abs(buffer.getSample(0, i)) + std::abs(buffer.getSample(1, i));
        for (int i = 14000; i < 22000; ++i)
            late += std::abs(buffer.getSample(0, i)) + std::abs(buffer.getSample(1, i));
        return std::pair{ early, late };
    };
    const auto base = renderEnergy(0.0f, 0.0f, 0.0f);
    const auto spread = renderEnergy(0.35f, 0.0f, 0.0f);
    const auto thump = renderEnergy(0.0f, 0.85f, 0.0f);
    const auto echo = renderEnergy(0.0f, 0.0f, 0.65f);
    CHECK(std::abs(spread.first - base.first) > 0.05);
    CHECK(std::abs(thump.first - base.first) > 10.0);
    CHECK(echo.second > base.second + 1.0);
}

namespace {

void analysisFft(std::vector<std::complex<float>>& data) {
    const size_t n = data.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -6.28318530718f / static_cast<float>(len);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                const auto u = data[i + j];
                const auto v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

float tailFlatness(const juce::AudioBuffer<float>& buffer) {
    constexpr int nfft = 2048;
    constexpr int start = 2400;
    std::vector<std::complex<float>> data(nfft);
    for (int i = 0; i < nfft; ++i) {
        const int index = start + i;
        const float sample = index < buffer.getNumSamples()
            ? 0.5f * (buffer.getSample(0, index) + buffer.getSample(1, index)) : 0.0f;
        const float window = 0.5f - 0.5f * std::cos(6.28318530718f * static_cast<float>(i) / static_cast<float>(nfft - 1));
        data[static_cast<size_t>(i)] = sample * window;
    }
    analysisFft(data);
    double logSum = 0.0;
    double sum = 0.0;
    int count = 0;
    const int from = std::max(2, static_cast<int>(150.0 * nfft / 48000.0));
    const int to = static_cast<int>(8000.0 * nfft / 48000.0);
    for (int bin = from; bin < to; ++bin) {
        const double magnitude = std::max(1.0e-12, static_cast<double>(std::abs(data[static_cast<size_t>(bin)])));
        logSum += std::log(magnitude);
        sum += magnitude;
        ++count;
    }
    return count > 0 && sum > 0.0 ? static_cast<float>(std::exp(logSum / count) / (sum / count)) : 1.0f;
}

std::vector<float> hitSpectrum(const juce::AudioBuffer<float>& buffer) {
    constexpr int nfft = 2048;
    std::vector<std::complex<float>> data(nfft);
    for (int i = 0; i < nfft; ++i) {
        const float sample = 0.5f * (buffer.getSample(0, i) + buffer.getSample(1, i));
        const float window = 0.5f - 0.5f * std::cos(6.28318530718f * static_cast<float>(i) / static_cast<float>(nfft - 1));
        data[static_cast<size_t>(i)] = sample * window;
    }
    analysisFft(data);
    std::vector<float> magnitude(nfft / 2);
    for (int i = 0; i < nfft / 2; ++i)
        magnitude[static_cast<size_t>(i)] = std::abs(data[static_cast<size_t>(i)]);
    return magnitude;
}

float spectrumCosine(const std::vector<float>& a, const std::vector<float>& b) {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i = 1; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        na += static_cast<double>(a[i]) * a[i];
        nb += static_cast<double>(b[i]) * b[i];
    }
    return na > 0.0 && nb > 0.0 ? static_cast<float>(dot / std::sqrt(na * nb)) : 0.0f;
}

juce::AudioBuffer<float> renderFactoryHit(OpenSmackerProcessor& processor) {
    constexpr int rate = 48000;
    constexpr int samples = rate / 2;
    processor.prepareToPlay(rate, samples);
    juce::AudioBuffer<float> buffer(2, samples);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(110)), 0);
    processor.processBlock(buffer, midi);
    return buffer;
}

} // namespace

TEST_CASE("Factory hits keep headroom, no DC, and a limiter that is only a ceiling", "[plugin][audio][level]") {
    OpenSmackerProcessor processor;
    auto* limiter = processor.parameters.getParameter(opensmacker::params::kLimiterEnabled);
    REQUIRE(limiter != nullptr);
    for (int index = 0; index < processor.getNumPrograms(); ++index) {
        processor.setCurrentProgram(index);
        CAPTURE(processor.getProgramName(index).toStdString());
        const auto limited = renderFactoryHit(processor);
        const float peak = limited.getMagnitude(0, limited.getNumSamples());
        CHECK(peak > 0.42f);
        CHECK(peak < 0.68f);
        double mean = 0.0;
        for (int i = 0; i < limited.getNumSamples(); ++i) {
            const float left = limited.getSample(0, i);
            const float right = limited.getSample(1, i);
            REQUIRE((std::isfinite(left) && std::isfinite(right)));
            CHECK(std::abs(left) < 0.98f);
            CHECK(std::abs(right) < 0.98f);
            mean += 0.5 * (left + right);
        }
        CHECK(std::abs(mean / limited.getNumSamples()) < 0.0015);
        CHECK(limited.getRMSLevel(0, 960, 960) > 0.025f);

        limiter->setValueNotifyingHost(0.0f);
        const auto open = renderFactoryHit(processor);
        const float openPeak = open.getMagnitude(0, open.getNumSamples());
        const float limitedDb = 20.0f * std::log10(std::max(peak, 1.0e-7f));
        const float openDb = 20.0f * std::log10(std::max(openPeak, 1.0e-7f));
        CHECK(std::abs(openDb - limitedDb) < 0.35f);
    }
}

TEST_CASE("Factory snares and rims are not sustaining bells, and presets stay distinct", "[plugin][audio][tone]") {
    OpenSmackerProcessor processor;
    std::vector<std::vector<float>> spectra;
    spectra.reserve(static_cast<size_t>(processor.getNumPrograms()));
    float closest = 0.0f;
    for (int index = 0; index < processor.getNumPrograms(); ++index) {
        processor.setCurrentProgram(index);
        const auto buffer = renderFactoryHit(processor);
        const auto family = opensmacker::presets::familyForProgram(index);
        const float flatness = tailFlatness(buffer);
        CAPTURE(processor.getProgramName(index).toStdString(), flatness, family);
        if (family == 0 || family == 3)
            CHECK(flatness > 0.18f);
        if (family == 1)
            CHECK(flatness < 0.28f);
        if (family == 2)
            CHECK(flatness > 0.06f);
        spectra.push_back(hitSpectrum(buffer));
    }
    for (size_t i = 0; i < spectra.size(); ++i) {
        for (size_t j = i + 1; j < spectra.size(); ++j)
            closest = std::max(closest, spectrumCosine(spectra[i], spectra[j]));
    }
    CHECK(closest < 0.992f);
}

TEST_CASE("A saved factoryBank 2 session keeps the pre-shaped voice until a new preset is chosen", "[plugin][state]") {
    OpenSmackerProcessor source;
    source.setCurrentProgram(4);
    auto tree = source.parameters.copyState();
    tree.removeProperty("presetName", nullptr);
    tree.setProperty("factoryBank", 2, nullptr);
    tree.setProperty("customSound", false, nullptr);
    juce::MemoryBlock state;
    if (auto xml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*xml, state);
    OpenSmackerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.currentSettings().synth.articulatedModes);
    CHECK_FALSE(restored.currentSettings().synth.shapedEngine);
    CHECK(restored.presetName() == restored.getProgramName(4));

    OpenSmackerProcessor modern;
    modern.setCurrentProgram(2);
    juce::MemoryBlock roundTrip;
    modern.getStateInformation(roundTrip);
    OpenSmackerProcessor reopened;
    reopened.setStateInformation(roundTrip.getData(), static_cast<int>(roundTrip.getSize()));
    CHECK(reopened.currentSettings().synth.shapedEngine);
    CHECK(reopened.currentSettings().synth.articulatedModes);
}
