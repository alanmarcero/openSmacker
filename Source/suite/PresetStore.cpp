#include "PresetStore.h"

namespace suite {

PresetStore::PresetStore(juce::String pluginName, juce::File documentsRoot)
    : pluginName_(std::move(pluginName)), documentsRoot_(std::move(documentsRoot)) {}

juce::File PresetStore::presetsDirectory() const {
    const auto documents = documentsRoot_.getFullPathName().isNotEmpty()
        ? documentsRoot_
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    return documents.getChildFile("Alan Marcero").getChildFile(pluginName_).getChildFile("Presets");
}

juce::File PresetStore::favoritesFile() const {
    return presetsDirectory().getParentDirectory().getChildFile("favorites.txt");
}

juce::String PresetStore::cleanName(juce::String name) {
    name = name.trim();
    juce::String clean;
    for (auto character : name) {
        const bool keep = juce::CharacterFunctions::isLetterOrDigit(character)
            || character == ' ' || character == '-' || character == '_'
            || character == '&' || character == '(' || character == ')'
            || character == '+' || character == '#';
        if (keep)
            clean += character;
    }
    clean = clean.trim();
    while (clean.startsWithChar('.'))
        clean = clean.substring(1).trim();
    if (clean.length() > 64)
        clean = clean.substring(0, 64).trim();
    if (clean.equalsIgnoreCase("favorites"))
        return {};
    return clean;
}

juce::String PresetStore::factoryKey(const juce::String& name) { return "F:" + name; }

juce::String PresetStore::userKey(const juce::String& name) { return "U:" + name; }

bool PresetStore::matchesQuery(const juce::String& name, const juce::String& query) {
    const auto needle = query.trim();
    return needle.isEmpty() || name.containsIgnoreCase(needle);
}

juce::File PresetStore::fileFor(const juce::String& name) const {
    const auto clean = cleanName(name);
    if (clean.isEmpty())
        return {};
    return presetsDirectory().getChildFile(clean + ".preset");
}

bool PresetStore::saveUserPreset(const juce::String& name, const juce::String& xml) const {
    const auto file = fileFor(name);
    if (file == juce::File{} || xml.isEmpty())
        return false;
    file.getParentDirectory().createDirectory();
    return file.replaceWithText(xml);
}

bool PresetStore::renameUserPreset(const juce::String& oldName, const juce::String& newName) const {
    const auto source = fileFor(oldName);
    const auto target = fileFor(newName);
    if (source == juce::File{} || target == juce::File{} || !source.existsAsFile())
        return false;
    if (source == target)
        return true;
    if (target.exists())
        return false;
    const auto wasFavorite = isFavorite(userKey(cleanName(oldName)));
    if (!source.moveFileTo(target))
        return false;
    if (wasFavorite) {
        setFavorite(userKey(cleanName(oldName)), false);
        setFavorite(userKey(cleanName(newName)), true);
    }
    return true;
}

bool PresetStore::deleteUserPreset(const juce::String& name) const {
    const auto file = fileFor(name);
    if (file == juce::File{} || !file.existsAsFile())
        return false;
    setFavorite(userKey(cleanName(name)), false);
    return file.deleteFile();
}

juce::String PresetStore::loadUserPreset(const juce::String& name) const {
    const auto file = fileFor(name);
    if (file == juce::File{} || !file.existsAsFile())
        return {};
    return file.loadFileAsString();
}

juce::StringArray PresetStore::userPresetNames() const {
    juce::StringArray names;
    const auto directory = presetsDirectory();
    if (!directory.isDirectory())
        return names;
    for (const auto& file : directory.findChildFiles(juce::File::findFiles, false, "*.preset"))
        names.add(file.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

juce::StringArray PresetStore::readFavorites() const {
    juce::StringArray keys;
    const auto file = favoritesFile();
    if (!file.existsAsFile())
        return keys;
    file.readLines(keys);
    keys.removeEmptyStrings();
    return keys;
}

void PresetStore::writeFavorites(const juce::StringArray& keys) const {
    auto file = favoritesFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(keys.joinIntoString("\n"));
}

bool PresetStore::isFavorite(const juce::String& key) const {
    return key.isNotEmpty() && readFavorites().contains(key);
}

void PresetStore::setFavorite(const juce::String& key, bool favorite) const {
    if (key.isEmpty())
        return;
    auto keys = readFavorites();
    const bool has = keys.contains(key);
    if (favorite && !has)
        keys.add(key);
    if (!favorite && has)
        keys.removeString(key);
    writeFavorites(keys);
}

} // namespace suite
