#pragma once

// stl
#include <string>
#include <string_view>

namespace valet::term
{

// Color of a status verb, matching Cargo's palette.
enum class Style {
	Green,	// progress / success (Compiling, Finished, Running)
	Cyan,	// informational (Installed, Fresh)
	Red,	// failures
	Yellow, // warnings
};

// Returns true when colored output should be emitted, i.e. stdout is a
// terminal and the NO_COLOR environment variable is not set.
bool color_enabled();

// Prints a Cargo/uv-style status line: a bold, colored, right-aligned (12
// column) verb followed by the message, e.g. "   Compiling src/main.cxx".
// Thread-safe; lines are not interleaved across concurrent calls.
void status(std::string_view verb, std::string_view message, Style style = Style::Green);

} // namespace valet::term
