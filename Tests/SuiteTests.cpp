#include "EditorSizing.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "PresetStore.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>

namespace {
float normalised(OpenSmackerProcessor& processor, const char* id) {
    return processor.parameters.getParameter(id)->getValue();
}

void gestureTo(OpenSmackerProcessor& processor, const char* id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(value);
    parameter->endChangeGesture();
}

bool wavHasEnergy(const std::vector<uint8_t>& wav) {
    if (wav.size() < 80 || std::memcmp(wav.data(), "RIFF", 4) != 0 || std::memcmp(wav.data() + 8, "WAVE", 4) != 0)
        return false;
    float peak = 0.0f;
    for (size_t i = 44; i + 4 <= wav.size(); i += 4) {
        float sample = 0.0f;
        std::memcpy(&sample, wav.data() + i, sizeof(float));
        if (std::isfinite(sample))
            peak = std::max(peak, std::abs(sample));
    }
    return peak > 0.001f;
}
}

TEST_CASE("Parameter gestures undo and redo as one step", "[plugin][suite][undo]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    const float original = normalised(processor, opensmacker::params::kOutputGain);
    gestureTo(processor, opensmacker::params::kOutputGain, 0.2f);
    const float first = normalised(processor, opensmacker::params::kOutputGain);
    gestureTo(processor, opensmacker::params::kOutputGain, 0.85f);
    REQUIRE(processor.canUndo());
    REQUIRE(processor.undo());
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(first));
    REQUIRE(processor.undo());
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(original));
    CHECK_FALSE(processor.canUndo());
    REQUIRE(processor.redo());
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(first));
    REQUIRE(processor.canRedo());
}

TEST_CASE("One slider gesture is a single undo step", "[plugin][suite][undo]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    const float original = normalised(processor, opensmacker::params::kToneDecay);
    auto* decay = processor.parameters.getParameter(opensmacker::params::kToneDecay);
    decay->beginChangeGesture();
    decay->setValueNotifyingHost(0.25f);
    decay->setValueNotifyingHost(0.7f);
    const float snapped = decay->getValue();
    decay->endChangeGesture();
    REQUIRE(processor.undo());
    CHECK(normalised(processor, opensmacker::params::kToneDecay) == Catch::Approx(original));
    CHECK_FALSE(processor.canUndo());
    REQUIRE(processor.redo());
    CHECK(normalised(processor, opensmacker::params::kToneDecay) == Catch::Approx(snapped));
}

TEST_CASE("A factory preset load undoes in one step", "[plugin][suite][undo]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    processor.setCurrentProgram(5);
    REQUIRE(processor.getCurrentProgram() == 5);
    REQUIRE(processor.canUndo());
    REQUIRE(processor.undo());
    CHECK(processor.getCurrentProgram() == 0);
    CHECK_FALSE(processor.canUndo());
    REQUIRE(processor.redo());
    CHECK(processor.getCurrentProgram() == 5);
}

TEST_CASE("Editor shortcuts and buttons undo parameter edits", "[plugin][suite][undo][gui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    auto editor = std::unique_ptr<OpenSmackerEditor>(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    const float original = normalised(processor, opensmacker::params::kStereoWidth);
    gestureTo(processor, opensmacker::params::kStereoWidth, 0.15f);
    const auto undoKey = juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0);
    const auto redoKey = juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0);
    REQUIRE(editor->keyPressed(undoKey));
    CHECK(normalised(processor, opensmacker::params::kStereoWidth) == Catch::Approx(original));
    REQUIRE(editor->keyPressed(redoKey));
    CHECK(normalised(processor, opensmacker::params::kStereoWidth) == Catch::Approx(0.15f));

    for (auto* child : editor->getChildren()) {
        if (auto* button = dynamic_cast<juce::TextButton*>(child); button != nullptr && button->getButtonText() == "Undo") {
            button->onClick();
            break;
        }
    }
    CHECK(normalised(processor, opensmacker::params::kStereoWidth) == Catch::Approx(original));
}

TEST_CASE("A/B slots keep two full sounds and copy either way", "[plugin][suite][ab]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    const float initial = normalised(processor, opensmacker::params::kOutputGain);
    gestureTo(processor, opensmacker::params::kOutputGain, 0.22f);
    const float tweaked = normalised(processor, opensmacker::params::kOutputGain);
    processor.selectAbSlot(1);
    CHECK(processor.abSlot() == 1);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(initial));
    processor.selectAbSlot(0);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(tweaked));
    processor.copyAbSlot(0, 1);
    processor.selectAbSlot(1);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(tweaked));
    gestureTo(processor, opensmacker::params::kOutputGain, 0.91f);
    const float onB = normalised(processor, opensmacker::params::kOutputGain);
    processor.copyAbSlot(1, 0);
    processor.selectAbSlot(0);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(onB));

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    OpenSmackerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(restored.abSlot() == 0);
    CHECK(normalised(restored, opensmacker::params::kOutputGain) == Catch::Approx(onB));
    restored.selectAbSlot(1);
    CHECK(normalised(restored, opensmacker::params::kOutputGain) == Catch::Approx(onB));
}

TEST_CASE("Each A/B slot keeps its own undo history", "[plugin][suite][ab][undo]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    const float initial = normalised(processor, opensmacker::params::kOutputGain);
    gestureTo(processor, opensmacker::params::kOutputGain, 0.2f);
    const float onA = normalised(processor, opensmacker::params::kOutputGain);

    processor.selectAbSlot(1);
    CHECK_FALSE(processor.canUndo());
    gestureTo(processor, opensmacker::params::kOutputGain, 0.7f);
    REQUIRE(processor.undo());
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(initial));
    CHECK_FALSE(processor.canUndo());

    processor.selectAbSlot(0);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(onA));
    REQUIRE(processor.undo());
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(initial));
    CHECK_FALSE(processor.canUndo());
    REQUIRE(processor.redo());

    processor.copyAbSlot(0, 1);
    processor.selectAbSlot(1);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(onA));
    CHECK_FALSE(processor.canUndo());
    CHECK_FALSE(processor.canRedo());
}

TEST_CASE("Editor size is resizable, unlocked, and restored with the session", "[plugin][suite][editor]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenSmackerProcessor processor;
    processor.setEditorSize(1480, 900);
    auto editor = std::unique_ptr<OpenSmackerEditor>(static_cast<OpenSmackerEditor*>(processor.createEditor()));
    REQUIRE(editor->getWidth() == 1480);
    REQUIRE(editor->getHeight() == 900);
    auto* constrainer = editor->getConstrainer();
    REQUIRE(constrainer != nullptr);
    CHECK(constrainer->getFixedAspectRatio() == 0.0);
    CHECK(constrainer->getMinimumWidth() == suite::kMinEditorWidth);
    CHECK(constrainer->getMinimumHeight() == suite::kMinEditorHeight);
    CHECK(constrainer->getMinimumWidth() >= 1000);
    CHECK(constrainer->getMinimumHeight() >= 700);
    editor->setSize(1600, 860);
    CHECK(editor->getWidth() == 1600);
    CHECK(editor->getHeight() == 860);
    CHECK(processor.editorWidth() == 1600);
    CHECK(processor.editorHeight() == 860);

    std::vector<juce::Rectangle<int>> targets;
    for (auto* child : editor->getChildren())
        CHECK(editor->getLocalBounds().contains(child->getBounds()));

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    OpenSmackerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    auto again = std::unique_ptr<OpenSmackerEditor>(static_cast<OpenSmackerEditor*>(restored.createEditor()));
    CHECK(again->getWidth() == 1600);
    CHECK(again->getHeight() == 860);

    OpenSmackerProcessor legacy;
    legacy.setCurrentProgram(2);
    auto tree = legacy.parameters.copyState();
    tree.setProperty("factoryBank", 2, nullptr);
    tree.setProperty("customSound", false, nullptr);
    juce::MemoryBlock oldSession;
    if (auto xml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*xml, oldSession);
    OpenSmackerProcessor opened;
    opened.setEditorSize(1300, 800);
    opened.setStateInformation(oldSession.getData(), static_cast<int>(oldSession.getSize()));
    CHECK(opened.editorWidth() == 1300);
    CHECK(opened.editorHeight() == 800);
}

TEST_CASE("Drag export uses the locked hit, otherwise a fresh render", "[plugin][suite][wav]") {
    OpenSmackerProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    auto* lock = processor.parameters.getParameter(opensmacker::params::kLockIn);
    REQUIRE(lock != nullptr);
    lock->setValueNotifyingHost(1.0f);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    for (int callback = 0; callback < 4000 && !processor.isSmackLockedIn(); ++callback)
        processor.processBlock(buffer, midi);
    REQUIRE(processor.isSmackLockedIn());
    const auto locked = processor.exportHitWav();
    REQUIRE(wavHasEnergy(locked));
    auto* decay = processor.parameters.getParameter(opensmacker::params::kToneDecay);
    decay->setValueNotifyingHost(decay->convertTo0to1(2400.0f));
    CHECK(processor.exportHitWav() == locked);

    lock->setValueNotifyingHost(0.0f);
    processor.processBlock(buffer, midi);
    REQUIRE_FALSE(processor.isSmackLockedIn());
    const auto fresh = processor.exportHitWav();
    REQUIRE(wavHasEnergy(fresh));
    CHECK(fresh != locked);

    const auto file = processor.createHitWavFile();
    REQUIRE(file.existsAsFile());
    CHECK(file.getFileExtension() == ".wav");
    juce::MemoryBlock disk;
    REQUIRE(file.loadFileAsData(disk));
    CHECK(disk.getSize() == fresh.size());
    file.deleteFile();
}

TEST_CASE("User presets save, search, favorite, rename, and delete without touching factory sounds", "[plugin][suite][presets]") {
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("opensmacker-suite-presets");
    root.deleteRecursively();
    suite::PresetStore store("openSmacker", root);
    CHECK(store.presetsDirectory() == root.getChildFile("Alan Marcero").getChildFile("openSmacker").getChildFile("Presets"));
    CHECK(store.favoritesFile() == root.getChildFile("Alan Marcero").getChildFile("openSmacker").getChildFile("favorites.txt"));
    REQUIRE(store.saveUserPreset("Room Tail", "<PRESET>room</PRESET>"));
    REQUIRE(store.saveUserPreset("Hall", "<PRESET>hall</PRESET>"));
    CHECK(store.userPresetNames().size() == 2);
    CHECK(store.loadUserPreset("Room Tail") == "<PRESET>room</PRESET>");
    CHECK(suite::PresetStore::matchesQuery("Room Tail", "room"));
    CHECK_FALSE(suite::PresetStore::matchesQuery("Hall", "room"));
    store.setFavorite(suite::PresetStore::userKey("Hall"), true);
    store.setFavorite(suite::PresetStore::factoryKey("Uplifting / Backbeat Snare"), true);
    suite::PresetStore reopened("openSmacker", root);
    CHECK(reopened.isFavorite(suite::PresetStore::userKey("Hall")));
    CHECK(reopened.isFavorite(suite::PresetStore::factoryKey("Uplifting / Backbeat Snare")));
    REQUIRE(store.renameUserPreset("Room Tail", "Close Room"));
    CHECK(store.loadUserPreset("Close Room") == "<PRESET>room</PRESET>");
    CHECK(store.loadUserPreset("Room Tail").isEmpty());
    CHECK_FALSE(store.deleteUserPreset("Uplifting / Backbeat Snare"));
    CHECK_FALSE(store.renameUserPreset("Uplifting / Backbeat Snare", "Renamed Factory"));
    REQUIRE(store.deleteUserPreset("Close Room"));
    CHECK(store.userPresetNames().size() == 1);

    juce::ScopedJuceInitialiser_GUI gui;
    std::vector<suite::PresetBrowser::FactoryPreset> factory{{ "Uplifting / Backbeat Snare", 0 }, { "Uplifting / Glass Pin Bell", 6 }};
    int loadedFactory = -1;
    juce::String loadedXml;
    suite::PresetBrowser browser(store, factory,
        [&](int index) { loadedFactory = index; },
        [&](const juce::String&, const juce::String& xml) { loadedXml = xml; },
        [&](const juce::String& name) { return store.saveUserPreset(name, "<PRESET>live</PRESET>"); });
    browser.setSize(360, 440);
    auto* search = [&]() -> juce::TextEditor* {
        for (auto* child : browser.getChildren())
            if (auto* editor = dynamic_cast<juce::TextEditor*>(child); editor != nullptr && editor->getName() == "Preset search")
                return editor;
        return nullptr;
    }();
    auto* list = [&]() -> juce::ListBox* {
        for (auto* child : browser.getChildren())
            if (auto* box = dynamic_cast<juce::ListBox*>(child))
                return box;
        return nullptr;
    }();
    auto* deleteButton = [&]() -> juce::TextButton* {
        for (auto* child : browser.getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*>(child); button != nullptr && button->getButtonText() == "Delete")
                return button;
        return nullptr;
    }();
    REQUIRE(search != nullptr);
    REQUIRE(list != nullptr);
    REQUIRE(deleteButton != nullptr);
    CHECK(browser.visiblePresetCount() == 3);
    browser.setSearchText("glass");
    CHECK(browser.visiblePresetCount() == 1);
    browser.setSearchText({});
    CHECK(browser.visiblePresetCount() == 3);
    list->selectRow(0);
    CHECK_FALSE(deleteButton->isEnabled());
    deleteButton->onClick();
    CHECK(store.userPresetNames().size() == 1);
    CHECK(loadedFactory == -1);
}

TEST_CASE("Loading a user preset restores the sound and leaves factory programs unchanged", "[plugin][suite][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("opensmacker-suite-load");
    root.deleteRecursively();
    OpenSmackerProcessor processor;
    processor.setPresetDocumentsRoot(root);
    processor.setCurrentProgram(4);
    REQUIRE(processor.canUndo());
    const auto factoryName = processor.getProgramName(4);
    gestureTo(processor, opensmacker::params::kOutputGain, 0.17f);
    const float savedGain = normalised(processor, opensmacker::params::kOutputGain);
    REQUIRE(processor.saveUserPreset("My Pocket"));
    processor.setCurrentProgram(0);
    REQUIRE(processor.canUndo());
    REQUIRE(processor.loadUserPreset("My Pocket"));
    CHECK(normalised(processor, opensmacker::params::kOutputGain) == Catch::Approx(savedGain));
    CHECK(processor.presetName() == "My Pocket");
    CHECK(processor.currentSettings().synth.shapedEngine);
    CHECK(processor.currentSettings().synth.articulatedModes);
    juce::MemoryBlock session;
    processor.getStateInformation(session);
    OpenSmackerProcessor reopened;
    reopened.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    CHECK(reopened.presetName() == "My Pocket");
    CHECK(reopened.currentSettings().synth.shapedEngine);
    REQUIRE(processor.undo());
    CHECK(processor.getCurrentProgram() == 0);
    processor.setCurrentProgram(4);
    CHECK(processor.getProgramName(4) == factoryName);
    CHECK(normalised(processor, opensmacker::params::kOutputGain) != Catch::Approx(savedGain));
    REQUIRE(processor.deleteUserPreset("My Pocket"));
    CHECK_FALSE(processor.loadUserPreset("My Pocket"));
    root.deleteRecursively();
}

TEST_CASE("A user preset saved by an earlier build on the legacy model stays on it", "[plugin][suite][presets]") {
    OpenSmackerProcessor source;
    auto tree = source.parameters.copyState();
    tree.setProperty("factoryBank", 2, nullptr);
    tree.setProperty("customSound", true, nullptr);
    tree.setProperty("synthModel", 2, nullptr);
    OpenSmackerProcessor processor;
    REQUIRE(processor.loadUserPresetXml("Old Kit", tree.createXml()->toString()));
    CHECK(processor.presetName() == "Old Kit");
    CHECK_FALSE(processor.currentSettings().synth.articulatedModes);
    CHECK_FALSE(processor.currentSettings().synth.shapedEngine);
}

TEST_CASE("The preset name follows loads, edits, saves, A/B slots, undo and sessions", "[plugin][suite][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("opensmacker-suite-names");
    root.deleteRecursively();
    OpenSmackerProcessor processor;
    processor.setPresetDocumentsRoot(root);
    processor.setCurrentProgram(3);
    CHECK(processor.presetName() == processor.getProgramName(3));
    CHECK_FALSE(processor.isPresetEdited());

    gestureTo(processor, opensmacker::params::kOutputGain, 0.3f);
    CHECK(processor.isPresetEdited());
    REQUIRE(processor.undo());
    CHECK_FALSE(processor.isPresetEdited());
    REQUIRE(processor.redo());
    CHECK(processor.isPresetEdited());

    REQUIRE(processor.saveUserPreset("Night Kit"));
    CHECK(processor.presetName() == "Night Kit");
    CHECK_FALSE(processor.isPresetEdited());
    gestureTo(processor, opensmacker::params::kOutputGain, 0.4f);
    CHECK(processor.isPresetEdited());

    processor.selectAbSlot(1);
    CHECK(processor.presetName() == processor.getProgramName(0));
    CHECK_FALSE(processor.isPresetEdited());
    processor.selectAbSlot(0);
    CHECK(processor.presetName() == "Night Kit");
    CHECK(processor.isPresetEdited());

    juce::MemoryBlock session;
    processor.getStateInformation(session);
    OpenSmackerProcessor reopened;
    reopened.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    CHECK(reopened.presetName() == "Night Kit");
    CHECK(reopened.isPresetEdited());
    root.deleteRecursively();
}
