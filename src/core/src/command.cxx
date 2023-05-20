#include "valet/command.hxx"

// valet
#include "valet/stopwatch.hxx"

// External
#include <spdlog/spdlog.h>

// stl
#include <cstdlib>

namespace valet
{

int execute(Command const& command, std::optional<std::filesystem::path> const& working_dir)
{
	util::Stopwatch stopwatch;
	std::string cmd;
	if (working_dir) {
		cmd = "cd " + working_dir->generic_string() + " && " + command.cmd;
	} else {
		cmd = command.cmd;
	}
	spdlog::trace("Executing: {}", cmd);
	auto ret = std::system(cmd.c_str());
	if (ret) {
		spdlog::error("Command failed with return code {}", ret);
	}
	spdlog::trace("Command took {}", stopwatch.elapsed_str());
	return ret;
}

} // namespace valet