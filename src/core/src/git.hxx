#pragma once

// stl
#include <string>
#include <optional>
#include <filesystem>

namespace valet
{

struct git_info {
	std::string name;
	std::string remote_url;
	std::optional<std::string> branch;
	std::optional<std::string> rev;
};

bool prepare_git_dep(std::filesystem::path const& dependant, git_info const& info,
		     std::filesystem::path& out_folder);

} // namespace valet