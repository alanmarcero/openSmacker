#pragma once

#include "PresetStore.h"
#include "SuiteStyle.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace suite {

class PresetBrowser : public juce::Component, private juce::ListBoxModel {
public:
    struct FactoryPreset {
        juce::String name;
        int index = 0;
    };

    PresetBrowser(PresetStore store,
                  std::vector<FactoryPreset> factory,
                  std::function<void(int index)> loadFactory,
                  std::function<void(const juce::String& xml)> loadUser,
                  std::function<juce::String()> captureXml,
                  Style style = {});

    void paint(juce::Graphics& g) override;
    void resized() override;
    [[nodiscard]] int visiblePresetCount() const { return static_cast<int>(visible_.size()); }
    void setSearchText(const juce::String& query);

private:
    struct Entry {
        juce::String name;
        bool factory = false;
        int factoryIndex = 0;
        bool favorite = false;
        juce::String key;
    };

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void rebuild();
    void updateButtons();
    void loadRow(int row);
    void toggleFavorite(int row);
    [[nodiscard]] const Entry* entry(int row) const;

    PresetStore store_;
    std::vector<FactoryPreset> factory_;
    std::function<void(int)> loadFactory_;
    std::function<void(const juce::String&)> loadUser_;
    std::function<juce::String()> captureXml_;
    Style style_;
    std::vector<Entry> visible_;

    juce::TextEditor search_;
    juce::TextEditor name_;
    juce::TextButton favoritesBtn_ { "Favorites" };
    juce::ListBox list_ { "Preset list", this };
    juce::TextButton saveBtn_ { "Save" };
    juce::TextButton renameBtn_ { "Rename" };
    juce::TextButton deleteBtn_ { "Delete" };
    bool favoritesOnly_ = false;
};

} // namespace suite
