#pragma once

#include <juce_graphics/juce_graphics.h>

namespace opensmacker::theme {

inline const juce::Colour kInk{ 0xff0d1317 };
inline const juce::Colour kSteel{ 0xff152026 };
inline const juce::Colour kEdge{ 0xff283842 };
inline const juce::Colour kSignalTrack{ 0xff283842 };
inline const juce::Colour kPaper{ 0xffe6f0f5 };
inline const juce::Colour kQuietText{ 0xff869ca8 };
inline const juce::Colour kSignalCyan{ 0xff48f0c8 };
inline const juce::Colour kImpactCoral = kSignalCyan;
inline const juce::Colour kBodyAmber{ 0xffffbe3b };
inline const juce::Colour kAirViolet = kSignalCyan;
inline const juce::Colour kAirMint = kSignalCyan;

inline const juce::Colour kBackground = kInk;
inline const juce::Colour kPanel = kSteel;
inline const juce::Colour kPanelEdge = kEdge;
inline const juce::Colour kKnobBody = kSteel;
inline const juce::Colour kTrack = kSignalTrack;
inline const juce::Colour kText = kPaper;
inline const juce::Colour kTextDim = kQuietText;
inline const juce::Colour kAccentMagenta = kImpactCoral;
inline const juce::Colour kAccentCyan = kSignalCyan;
inline const juce::Colour kAccentAmber = kBodyAmber;
inline const juce::Colour kAccentViolet = kAirViolet;
inline const juce::Colour kAccentNeonGreen = kAirMint;
inline const juce::Colour kAccentFlame = kImpactCoral;
inline const juce::Colour kAccentDim = kQuietText;

} // namespace opensmacker::theme
