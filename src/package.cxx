#include "package.hxx"

// std
#include <stack>
#include <unordered_set>

// external
#include <tomlplusplus/toml.hpp>
#include <spdlog/spdlog.h>

namespace std
{
template <>
struct hash<std::filesystem::path> {
	size_t operator()(std::filesystem::path const& path) const
	{
		return std::hash<std::string>()(std::filesystem::canonical(path));
	}
};
} // namespace std

namespace autob
{

std::optional<DependencyGraph<Package>>
make_package_graph(std::filesystem::path const& project_folder)
{
	auto package = find_package(project_folder); // TODO: Handle parse errors
	if (!package)
		return std::nullopt;
	DependencyGraph<Package> package_graph;
	package_graph.add(*package);

	// TODO: For remote dependencies, we need to download them before resolving.
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
		for (auto const& dep_info : resolved->dependencies)
			to_be_resolved.push({*resolved, dep_info});
	}

	return package_graph;
}

std::optional<Package> find_package(std::filesystem::path const& folder)
{
	spdlog::trace("Entering project folder: {}", folder.generic_string());
	for (auto const& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.path().extension() == ".toml") {
			return parse_package_cfg(entry.path()); // TODO: Handle parse errors
		}
	}
	spdlog::error("Cannot find autob config file under directory {}", folder.generic_string());
	return std::nullopt;
}

Package parse_package_cfg(std::filesystem::path const& cfg_file_path)
{
	// TODO: Exception handling
	auto package_folder = cfg_file_path.parent_path();
	auto cfg_tbl = toml::parse_file(cfg_file_path.generic_string());
	auto const& package_toml = cfg_tbl["package"];
	Package package;
	package.name = package_toml["name"].value_or<std::string>("");
	package.version = package_toml["version"].value_or<std::string>("");
	package.id = package.name + "=" + package.version;
	package.std = package_toml["std"].value_or<std::string>("");
	package.folder = package_folder;

	auto package_type_str = package_toml["type"].value_or<std::string>("");
	if (package_type_str == "bin")
		package.type = PackageType::Application;
	else if (package_type_str == "lib")
		package.type = PackageType::StaticLibrary;

	auto const& includes_ref = package_toml["includes"];
	if (auto path_arr = includes_ref.as_array()) {
		for (auto&& path_node : *path_arr) {
			std::filesystem::path path = *path_node.value<std::string>();
			path = std::filesystem::canonical(package_folder / path);
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
	if (!cfg_tbl.contains("dependencies")) {
		return package;
	}
	auto const& dependencies_toml = cfg_tbl["dependencies"];
	for (auto const& entry : *dependencies_toml.node()->as_table()) {
		DependencyInfo dep_info;
		// TODO: Include version info
		dep_info.name = entry.first.str();
		dep_info.folder =
		    std::filesystem::canonical(package_folder / entry.second.as_string()->get());
		package.dependencies.push_back(dep_info);
	}
	return package;
}

} // namespace autob
