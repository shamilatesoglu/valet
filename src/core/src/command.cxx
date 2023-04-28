#include "valet/command.hxx"

// valet
#include "valet/stopwatch.hxx"

// External
#include <spdlog/spdlog.h>

// stl
#include <cstdlib>

namespace valet
{

int execute(Command const& command)
{
	util::Stopwatch stopwatch;
	spdlog::trace("Executing: {}", command.cmd);
	auto ret = std::system(command.cmd.c_str());
	if (ret) {
		spdlog::error("Command failed with return code {}", ret);
	}
	spdlog::trace("Command took {}", stopwatch.elapsed_str());
	return ret;
}

} // namespace valet