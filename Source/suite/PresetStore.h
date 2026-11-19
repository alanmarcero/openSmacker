#pragma once

#include <juce_core/juce_core.h>

namespace suite {

// User presets live in ~/Documents/Alan Marcero/<plugin>/Presets.
// Favorites live beside that folder. Factory presets are not files here.
class PresetStore {
public:
    PresetStore(juce::String pluginName, juce::File documentsRoot = {});

    [[nodiscard]] juce::File presetsDirectory() const;
    [[nodiscard]] juce::File favoritesFile() const;

    [[nodiscard]] static juce::String cleanName(juce::String name);
    [[nodiscard]] static juce::String factoryKey(const juce::String& name);
    [[nodiscard]] static juce::String userKey(const juce::String& name);
    [[nodiscard]] static bool matchesQuery(const juce::String& name, const juce::String& query);

    [[nodiscard]] bool saveUserPreset(const juce::String& name, const juce::String& xml) const;
    [[nodiscard]] bool renameUserPreset(const juce::String& oldName, const juce::String& newName) const;
    [[nodiscard]] bool deleteUserPreset(const juce::String& name) const;
    [[nodiscard]] juce::String loadUserPreset(const juce::String& name) const;
    [[nodiscard]] juce::StringArray userPresetNames() const;

    [[nodiscard]] bool isFavorite(const juce::String& key) const;
    void setFavorite(const juce::String& key, bool favorite) const;

private:
    [[nodiscard]] juce::File fileFor(const juce::String& name) const;
    [[nodiscard]] juce::StringArray readFavorites() const;
    void writeFavorites(const juce::StringArray& keys) const;

    juce::String pluginName_;
    juce::File documentsRoot_;
};

} // namespace suite
