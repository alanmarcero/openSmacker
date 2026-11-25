#pragma once

#include "FamilyLayout.h"

namespace suite {

// The editor lays out at the default size, then scales from 70% to 200% with the aspect locked.
inline constexpr int kDefaultEditorWidth = kFamilyDesignWidth;
inline constexpr int kDefaultEditorHeight = 752;
inline constexpr int kMinEditorWidth = kDefaultEditorWidth * 7 / 10;
inline constexpr int kMinEditorHeight = kDefaultEditorHeight * 7 / 10;
inline constexpr int kMaxEditorWidth = kDefaultEditorWidth * 2;
inline constexpr int kMaxEditorHeight = kDefaultEditorHeight * 2;

} // namespace suite
