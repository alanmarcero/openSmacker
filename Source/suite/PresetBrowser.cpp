#include "PresetBrowser.h"

namespace suite {

PresetBrowser::PresetBrowser(PresetStore store,
                             std::vector<FactoryPreset> factory,
                             std::function<void(int)> loadFactory,
                             std::function<void(const juce::String&, const juce::String&)> loadUser,
                             std::function<bool(const juce::String&)> saveUser,
                             Style style)
    : store_(std::move(store)), factory_(std::move(factory)), loadFactory_(std::move(loadFactory)),
      loadUser_(std::move(loadUser)), saveUser_(std::move(saveUser)), style_(style) {
    search_.setName("Preset search");
    search_.setTextToShowWhenEmpty("Search presets", style_.dim);
    search_.setColour(juce::TextEditor::backgroundColourId, style_.ink);
    search_.setColour(juce::TextEditor::textColourId, style_.text);
    search_.setColour(juce::TextEditor::outlineColourId, style_.edge);
    search_.setColour(juce::TextEditor::focusedOutlineColourId, style_.accent);
    search_.onTextChange = [this] { rebuild(); };
    name_.setName("Preset name");
    name_.setTextToShowWhenEmpty("Preset name", style_.dim);
    name_.setColour(juce::TextEditor::backgroundColourId, style_.ink);
    name_.setColour(juce::TextEditor::textColourId, style_.text);
    name_.setColour(juce::TextEditor::outlineColourId, style_.edge);
    name_.setColour(juce::TextEditor::focusedOutlineColourId, style_.accent);

    favoritesBtn_.setClickingTogglesState(true);
    favoritesBtn_.setColour(juce::TextButton::buttonOnColourId, style_.accent);
    favoritesBtn_.setColour(juce::TextButton::textColourOnId, style_.ink);
    favoritesBtn_.onClick = [this] {
        favoritesOnly_ = favoritesBtn_.getToggleState();
        rebuild();
    };
    saveBtn_.onClick = [this] {
        const auto presetName = PresetStore::cleanName(name_.getText());
        if (presetName.isEmpty() || !saveUser_ || !saveUser_(presetName))
            return;
        name_.setText(presetName, juce::dontSendNotification);
        rebuild();
    };
    renameBtn_.onClick = [this] {
        const auto* selected = entry(list_.getSelectedRow());
        if (selected == nullptr || selected->factory)
            return;
        const auto presetName = PresetStore::cleanName(name_.getText());
        if (presetName.isEmpty())
            return;
        if (store_.renameUserPreset(selected->name, presetName)) {
            name_.setText(presetName, juce::dontSendNotification);
            rebuild();
        }
    };
    deleteBtn_.onClick = [this] {
        const auto* selected = entry(list_.getSelectedRow());
        if (selected == nullptr || selected->factory)
            return;
        if (store_.deleteUserPreset(selected->name)) {
            name_.clear();
            rebuild();
        }
    };
    for (auto* button : { &favoritesBtn_, &saveBtn_, &renameBtn_, &deleteBtn_ }) {
        button->setColour(juce::TextButton::buttonColourId, style_.panel);
        button->setColour(juce::TextButton::textColourOffId, style_.text);
    }
    list_.setColour(juce::ListBox::backgroundColourId, style_.panel);
    list_.setColour(juce::ListBox::outlineColourId, style_.edge);
    list_.setOutlineThickness(1);
    list_.setRowHeight(30);
    for (auto* child : std::initializer_list<juce::Component*>{
             &search_, &favoritesBtn_, &list_, &name_, &saveBtn_, &renameBtn_, &deleteBtn_ })
        addAndMakeVisible(child);
    rebuild();
}

void PresetBrowser::setSearchText(const juce::String& query) {
    search_.setText(query, juce::dontSendNotification);
    rebuild();
}

void PresetBrowser::paint(juce::Graphics& g) {
    g.fillAll(style_.background);
    g.setColour(style_.dim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Factory presets stay read-only", 12, getHeight() - 22, getWidth() - 24, 16,
               juce::Justification::centredLeft);
}

void PresetBrowser::resized() {
    auto area = getLocalBounds().reduced(10);
    area.removeFromBottom(18);
    auto buttons = area.removeFromBottom(32);
    const int gap = 6;
    const int buttonWidth = (buttons.getWidth() - gap * 2) / 3;
    saveBtn_.setBounds(buttons.removeFromLeft(buttonWidth));
    buttons.removeFromLeft(gap);
    renameBtn_.setBounds(buttons.removeFromLeft(buttonWidth));
    buttons.removeFromLeft(gap);
    deleteBtn_.setBounds(buttons);
    area.removeFromBottom(6);
    name_.setBounds(area.removeFromBottom(28));
    area.removeFromBottom(6);
    auto filters = area.removeFromTop(28);
    favoritesBtn_.setBounds(filters.removeFromRight(96));
    filters.removeFromRight(6);
    search_.setBounds(filters);
    area.removeFromTop(6);
    list_.setBounds(area);
}

int PresetBrowser::getNumRows() { return static_cast<int>(visible_.size()); }

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
    const auto* item = entry(row);
    if (item == nullptr)
        return;
    g.fillAll(selected ? style_.edge : style_.panel);
    g.setColour(item->favorite ? style_.accent : style_.dim);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(item->favorite ? "Star" : "Mark", width - 52, 0, 46, height, juce::Justification::centred);
    g.setColour(style_.text);
    g.setFont(juce::FontOptions(14.0f));
    const auto prefix = item->factory ? "Factory  " : "User  ";
    g.setColour(style_.dim);
    g.drawText(prefix, 8, 0, 62, height, juce::Justification::centredLeft);
    g.setColour(style_.text);
    g.drawText(item->name, 72, 0, width - 130, height, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& event) {
    const int rowWidth = event.eventComponent != nullptr ? event.eventComponent->getWidth() : list_.getWidth();
    if (event.x >= rowWidth - 58) {
        toggleFavorite(row);
        return;
    }
    if (const auto* item = entry(row))
        name_.setText(item->name, juce::dontSendNotification);
    updateButtons();
    loadRow(row);
}

void PresetBrowser::selectedRowsChanged(int) { updateButtons(); }

void PresetBrowser::rebuild() {
    const auto query = search_.getText();
    std::vector<Entry> favorites;
    std::vector<Entry> rest;
    auto push = [&](Entry entry) {
        if (!PresetStore::matchesQuery(entry.name, query))
            return;
        entry.favorite = store_.isFavorite(entry.key);
        if (favoritesOnly_ && !entry.favorite)
            return;
        (entry.favorite ? favorites : rest).push_back(std::move(entry));
    };
    for (const auto& preset : factory_)
        push({ preset.name, true, preset.index, false, PresetStore::factoryKey(preset.name) });
    for (const auto& presetName : store_.userPresetNames())
        push({ presetName, false, -1, false, PresetStore::userKey(presetName) });
    visible_ = std::move(favorites);
    visible_.insert(visible_.end(), rest.begin(), rest.end());
    list_.updateContent();
    updateButtons();
    repaint();
}

void PresetBrowser::updateButtons() {
    const auto* selected = entry(list_.getSelectedRow());
    const bool user = selected != nullptr && !selected->factory;
    renameBtn_.setEnabled(user);
    deleteBtn_.setEnabled(user);
    favoritesBtn_.setToggleState(favoritesOnly_, juce::dontSendNotification);
}

void PresetBrowser::loadRow(int row) {
    const auto* item = entry(row);
    if (item == nullptr)
        return;
    if (item->factory) {
        if (loadFactory_)
            loadFactory_(item->factoryIndex);
        return;
    }
    const auto xml = store_.loadUserPreset(item->name);
    if (xml.isNotEmpty() && loadUser_)
        loadUser_(item->name, xml);
}

void PresetBrowser::toggleFavorite(int row) {
    const auto* item = entry(row);
    if (item == nullptr)
        return;
    store_.setFavorite(item->key, !item->favorite);
    rebuild();
}

const PresetBrowser::Entry* PresetBrowser::entry(int row) const {
    if (row < 0 || row >= static_cast<int>(visible_.size()))
        return nullptr;
    return &visible_[static_cast<size_t>(row)];
}

} // namespace suite
