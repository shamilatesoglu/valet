#include "git.hxx"

// valet
#include "platform.hxx"
#include "sha1.hxx"
#include "valet/command.hxx"

// External
#include <spdlog/spdlog.h>

namespace valet
{

std::string get_git_repo_name_from_url(std::string const& url)
{
	std::string repo_name = url;
	auto pos = repo_name.find_last_of('/');
	if (pos != std::string::npos) {
		repo_name = repo_name.substr(pos + 1);
	}
	pos = repo_name.find_last_of('.');
	if (pos != std::string::npos) {
		repo_name = repo_name.substr(0, pos);
	}
	return repo_name;
}

bool prepare_git_dep(std::filesystem::path const& dependant, const git_info& info,
		     std::filesystem::path& out_folder)
{
	// 1. Clone the repo to user's cache folder
	// 2. Checkout the branch/rev/tag
	// 3. (TODO) Make sure the folder contains a valet manifest.
	// 4. (TODO) Save the currently used revision to some 
	//    place private to the package that is being built, 
	//    so that it doesn't get fetched every time unless
	//    a 'valet update' is called.
	// 5. Return the folder path to the caller.

	std::string cmd = "git clone";
	if (info.branch) {
		cmd += " -b " + *info.branch;
	}
	cmd += " " + info.remote_url + " --recurse-submodules --depth=1";
	std::string hash = SHA1()(info.remote_url);
	std::filesystem::path clone_folder = platform::garage_dir() / hash;
	if (!std::filesystem::exists(platform::garage_dir())) {
		std::filesystem::create_directories(platform::garage_dir());
	}
	else if (std::filesystem::exists(clone_folder)) {
		spdlog::debug("Found {} in cache", info.remote_url);
		out_folder = clone_folder;
		return true;
	}
	cmd += " " + clone_folder.generic_string();
	spdlog::info("Cloning {} to {}", info.remote_url, clone_folder.generic_string());
	if (execute({cmd})) {
		spdlog::error("Failed to clone {} to {}", info.remote_url,
			      clone_folder.generic_string());
		return false;
	}
	out_folder = clone_folder;

	return true;
}

} // namespace valet