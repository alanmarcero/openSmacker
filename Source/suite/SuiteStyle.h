#pragma once

#include <juce_graphics/juce_graphics.h>

namespace suite {

struct Style {
    juce::Colour background { 0xff0d1317 };
    juce::Colour panel { 0xff152026 };
    juce::Colour ink { 0xff0a1014 };
    juce::Colour text { 0xffe6f0f5 };
    juce::Colour dim { 0xff869ca8 };
    juce::Colour accent { 0xffffbe3b };
    juce::Colour edge { 0xff283842 };
};

} // namespace suite
