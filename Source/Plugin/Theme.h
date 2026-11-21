#pragma once

#include <juce_graphics/juce_graphics.h>

namespace opensmacker::theme {

inline const juce::Colour kInk{ 0xff0a1014 };
inline const juce::Colour kBackground{ 0xff0d1317 };
inline const juce::Colour kPanel{ 0xff152026 };
inline const juce::Colour kKnobBody{ 0xff1b272d };
inline const juce::Colour kPanelEdge{ 0xff283842 };
inline const juce::Colour kTrack{ 0xff283842 };
inline const juce::Colour kText{ 0xffe6f0f5 };
inline const juce::Colour kTextDim{ 0xff869ca8 };
inline const juce::Colour kApricot{ 0xffffbe3b };
inline const juce::Colour kLilac{ 0xff48f0c8 };
inline const juce::Colour kIce{ 0xff5ce1ff };
// Controls that have no effect, and disabled controls, draw at this opacity.
inline constexpr float kInactiveOpacity = 0.4f;

} // namespace opensmacker::theme
