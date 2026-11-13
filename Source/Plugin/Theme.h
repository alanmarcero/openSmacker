#pragma once

#include <juce_graphics/juce_graphics.h>

namespace opensmacker::theme {

inline const juce::Colour kInk{ 0xff111824 };
inline const juce::Colour kSteel{ 0xff202b38 };
inline const juce::Colour kEdge{ 0xff344455 };
inline const juce::Colour kSignalTrack{ 0xff405366 };
inline const juce::Colour kPaper{ 0xffe9e4d8 };
inline const juce::Colour kQuietText{ 0xffa5b2bf };
inline const juce::Colour kSignalCyan{ 0xff4ec5e8 };
inline const juce::Colour kImpactCoral{ 0xffff715b };
inline const juce::Colour kBodyAmber{ 0xffe9b971 };
inline const juce::Colour kAirViolet{ 0xffaa9ac4 };
inline const juce::Colour kAirMint{ 0xff8fd3c1 };

inline const juce::Colour kBackground = kInk;
inline const juce::Colour kPanel = kSteel;
inline const juce::Colour kPanelEdge = kEdge;
inline const juce::Colour kKnobBody{ 0xff2a3746 };
inline const juce::Colour kTrack = kSignalTrack;
inline const juce::Colour kText = kPaper;
inline const juce::Colour kTextDim = kQuietText;
inline const juce::Colour kAccentMagenta = kImpactCoral;
inline const juce::Colour kAccentCyan = kSignalCyan;
inline const juce::Colour kAccentAmber = kBodyAmber;
inline const juce::Colour kAccentViolet = kAirViolet;
inline const juce::Colour kAccentNeonGreen = kAirMint;
inline const juce::Colour kAccentFlame = kImpactCoral;
inline const juce::Colour kAccentDim{ 0xff526273 };

} // namespace opensmacker::theme
