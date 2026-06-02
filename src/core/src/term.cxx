#include "valet/term.hxx"

// stl
#include <cstdio>
#include <cstdlib>
#include <format>
#include <mutex>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace valet::term
{

namespace
{

// Width of the verb column, matching Cargo ("   Compiling" is 12 columns).
constexpr int verb_width = 12;
constexpr const char* ansi_reset = "\033[0m";

const char* ansi_style(Style style)
{
	switch (style) {
	case Style::Green:
		return "\033[1;32m";
	case Style::Cyan:
		return "\033[1;36m";
	case Style::Red:
		return "\033[1;31m";
	case Style::Yellow:
		return "\033[1;33m";
	}
	return "";
}

bool stdout_is_tty()
{
#if defined(_WIN32)
	return _isatty(_fileno(stdout)) != 0;
#else
	return isatty(fileno(stdout)) != 0;
#endif
}

std::mutex print_mutex;

} // namespace

bool color_enabled()
{
	static const bool enabled = std::getenv("NO_COLOR") == nullptr && stdout_is_tty();
	return enabled;
}

void status(std::string_view verb, std::string_view message, Style style)
{
	int pad = verb_width - static_cast<int>(verb.size());
	std::string indent(pad > 0 ? pad : 0, ' ');
	std::string line =
	    color_enabled()
		? std::format("{}{}{}{} {}\n", indent, ansi_style(style), verb, ansi_reset, message)
		: std::format("{}{} {}\n", indent, verb, message);
	std::scoped_lock lock(print_mutex);
	std::fputs(line.c_str(), stdout);
	std::fflush(stdout);
}

} // namespace valet::term
