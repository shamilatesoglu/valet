#include "valet/package.hxx"

// valet
#include "valet/build.hxx"
#include "platform.hxx"
#include "git.hxx"

// external
#define TOML_EXCEPTIONS 0
#include <tomlplusplus/toml.hpp>
#include <spdlog/spdlog.h>

// stl
#include <stack>
#include <unordered_set>

#if __APPLE__
namespace std
{
template <>
struct hash<std::filesystem::path> {
	size_t operator()(std::filesystem::path const& path) const
	{
		return std::hash<std::string>()(std::filesystem::canonical(path).generic_string());
	}
};
} // namespace std
#endif

namespace valet
{

std::filesystem::path Package::target_path(bool release) const
{
	std::filesystem::path build_folder = get_build_folder(this->folder, release);
	return build_folder / this->id / (this->name + this->target_ext());
}

std::string Package::target_ext() const
{
	switch (this->type) {
	case Package::Type::Application:
		return platform::EXECUTABLE_EXT;
	case Package::Type::StaticLibrary:
		return platform::STATIC_LIB_EXT;
	case Package::Type::SharedLibrary:
		return platform::SHARED_LIB_EXT;
	default:
		return "";
	}
}

std::optional<DependencyGraph<Package>>
make_package_graph(std::filesystem::path const& project_folder)
{
	auto package = find_package(project_folder); // TODO: Handle parse errors
	if (!package)
		return std::nullopt;
	DependencyGraph<Package> package_graph;
	package_graph.add(*package);

	std::stack<std::pair<Package, DependencyInfo>> to_be_resolved;
	for (auto const& dep_info : package->dependencies)
		to_be_resolved.push({*package, dep_info});
	std::unordered_map<std::filesystem::path, Package> resolved_packages; // Path -> Package
	while (!to_be_resolved.empty()) {
		auto pair = to_be_resolved.top();
		to_be_resolved.pop();
		auto const& cur_dep_info = pair.second;
		auto const& cur_package = pair.first;
		std::optional<Package> resolved = std::nullopt;
		auto it = resolved_packages.find(cur_dep_info.folder);
		if (it == resolved_packages.end()) {
			resolved = find_package(cur_dep_info.folder);
		} else {
			resolved = it->second;
		}
		if (!resolved) {
			spdlog::error("Unable to resolve the dependency {} of {}",
				      cur_dep_info.name, cur_package.id);
			return std::nullopt; // continue?
		}
		package_graph.add(*resolved);
		package_graph.depend(cur_package, *resolved);
		resolved_packages[cur_dep_info.folder] = *resolved;
		for (auto const& dep_info : resolved->dependencies) {
			if (resolved_packages.find(dep_info.folder) == resolved_packages.end())
				to_be_resolved.push({*resolved, dep_info});
		}
	}

	return package_graph;
}

std::optional<Package> find_package(std::filesystem::path const& folder)
{
	spdlog::trace("Entering project folder: {}", folder.generic_string());
	for (auto const& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.path().filename() == "valet.toml") {
			return Package::parse_from(entry.path()); // TODO: Handle parse errors
		}
	}
	return std::nullopt;
}

std::optional<Package> Package::parse_from(std::filesystem::path const& manifest_fp)
{
	auto package_folder = manifest_fp.parent_path();
	auto parse_res = toml::parse_file(manifest_fp.generic_string());
	if (!parse_res) {
		spdlog::error("Failed to parse valet config file: {}",
			      manifest_fp.generic_string());
		return std::nullopt;
	}
	toml::table const& manifest = parse_res;
	auto const& package_toml = manifest["package"];
	Package package;
	package.name = package_toml["name"].value_or<std::string>("");
	package.version = package_toml["version"].value_or<std::string>("");
	package.id = package.name + "=" + package.version;
	package.std = package_toml["std"].value_or<std::string>("");
	package.folder = package_folder;

	auto package_type_str = package_toml["type"].value_or<std::string>("");
	auto package_type = get_package_type(package_type_str);
	if (!package_type) {
		spdlog::error("Invalid package type: {}", package_type_str);
		return std::nullopt;
	}
	package.type = *package_type;

	auto const& includes_ref = package_toml["includes"];
	if (auto path_arr = includes_ref.as_array()) {
		for (auto&& path_node : *path_arr) {
			std::filesystem::path path =
			    package_folder / *path_node.value<std::string>();
			if (!std::filesystem::exists(path)) {
				spdlog::error("{} public_includes: No such folder {}", package.name,
					      path.generic_string());
				return std::nullopt;
			}
			path = std::filesystem::canonical(path);
			package.includes.push_back(path);
		}
	}
	auto const& public_includes_ref = package_toml["public_includes"];
	if (auto path_arr = public_includes_ref.as_array()) {
		for (auto&& path_node : *path_arr) {
			std::filesystem::path path = *path_node.value<std::string>();
			path = std::filesystem::canonical(package_folder / path);
			package.public_includes.push_back(path);
		}
	}
	auto const& compile_options_ref = package_toml["compile_options"];
	if (auto options_arr = compile_options_ref.as_array()) {
		for (auto&& option_node : *options_arr) {
			package.compile_options.push_back(*option_node.value<std::string>());
		}
	}
	if (!manifest.contains("dependencies")) {
		return package;
	}
	auto const& dependencies_toml = manifest["dependencies"];
	for (auto const& entry : *dependencies_toml.node()->as_table()) {
		DependencyInfo dep_info;
		// TODO: Include version info
		dep_info.name = entry.first.str();
		// entry.second can either be
		// 1. "version_string",
		// 2. { path = "path_string" }
		// 3. { git = "url_string" }
		// We only support the 2nd & 3rd cases for now.
		bool ok = false;
		if (entry.second.is_table()) {
			auto const& dep_tbl = entry.second.as_table();
			if (dep_tbl->contains("path")) {
				dep_info.folder = std::filesystem::canonical(
				    package_folder / dep_tbl->at("path").as_string()->get());
				ok = true;
			} else if (dep_tbl->contains("git")) {
				git_info git;
				git.name = dep_info.name;
				git.remote_url = dep_tbl->at("git").as_string()->get();
				if (dep_tbl->contains("rev")) {
					git.rev = dep_tbl->at("rev").as_string()->get();
					spdlog::debug("Git dependency {}: Using rev {}", git.name,
						      git.rev);
				} else if (dep_tbl->contains("tag")) {
					git.rev = dep_tbl->at("tag").as_string()->get();
					spdlog::debug("Git dependency {}: Using tag {}", git.name,
						      git.rev);
				} else {
					spdlog::error(
					    "Git dependency {}: No revision or tag specified",
					    git.name);
					return std::nullopt;
				}
				std::filesystem::path out_folder;
				if (!prepare_git_dep(package_folder, git, out_folder)) {
					spdlog::error("Failed to prepare git dependency {}",
						      git.name);
					return std::nullopt;
				}
				dep_info.folder = out_folder;
				ok = true;
			}
		}
		if (entry.second.is_string()) {
			spdlog::error(
			    "Currently, valet only supports local and git dependencies. :(");
			return std::nullopt;
		}
		if (!ok) {
			spdlog::error("Invalid dependency: {}", entry.first.str());
			return std::nullopt;
		}
		package.dependencies.push_back(dep_info);
	}
	return package;
}

std::optional<Package::Type> get_package_type(const std::string& type)
{
	if (type == "bin")
		return Package::Type::Application;
	else if (type == "lib")
		return Package::Type::StaticLibrary;
	else if (type == "dylib")
		return Package::Type::SharedLibrary;
	else
		return std::nullopt;
}

} // namespace valet
