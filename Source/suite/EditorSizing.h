#pragma once

namespace suite {

// The editor lays out at the default size, then scales.
inline constexpr int kDefaultEditorWidth = 1120;
inline constexpr int kDefaultEditorHeight = 752;
inline constexpr int kMinEditorWidth = kDefaultEditorWidth * 7 / 10;
inline constexpr int kMinEditorHeight = kDefaultEditorHeight * 7 / 10;
inline constexpr int kMaxEditorWidth = 2200;
inline constexpr int kMaxEditorHeight = 1500;

} // namespace suite
