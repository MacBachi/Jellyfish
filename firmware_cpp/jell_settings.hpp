#pragma once
// The bloom's look, remembered across restarts: mode, brightness, hue, cycle, filament
// switch and sound sensitivity, plus this jelly's own microphone trim. Every jelly keeps a
// copy of the last look it showed (its own commands or the AP's STATE), so a jelly that
// restarts and ends up as the AP brings the look back instead of the defaults.
//
// Lives in the last flash sector (the SDK leaves it free; the linker keeps the program out
// of the last four, see CMakeLists.txt). Writes are rare: only after the values have stopped
// changing for SETTINGS_SETTLE_MS, at most once per SETTINGS_MIN_INTERVAL_MS, and only when
// something differs from what is stored. Core 1 parks between two frames for the write so
// the flash lockout never interrupts an LED push.
#include "jell_state.hpp"

namespace Settings
{
    // Restore the look and the trim from flash. Returns true when a valid record was found.
    bool load(JellState& state, float& trim);

    // The look changed (own command or applied STATE): remember it, written later by poll().
    void note(const JellState& state);

    // This jelly's microphone trim changed.
    void note_trim(float trim);

    // Core 0, from the main loop: writes a pending change once it has settled.
    void poll();
}
