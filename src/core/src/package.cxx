#include "valet/package.hxx"

// valet
#include "valet/build.hxx"
#include "valet/platform.hxx"

// external
#define TOML_EXCEPTIONS 0
#include <tomlplusplus/toml.hpp>
#include <spdlog/spdlog.h>

// stl
#include <stack>
#include <unordered_set>

#if not _WIN32
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
	case PackageType::Application:
		return platform::EXECUTABLE_EXT;
	case PackageType::StaticLibrary:
		return platform::STATIC_LIB_EXT;
	case PackageType::SharedLibrary:
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
		if (entry.path().filename() == "valet.toml") {
			return parse_package_cfg(entry.path()); // TODO: Handle parse errors
		}
	}
	return std::nullopt;
}

std::optional<Package> parse_package_cfg(std::filesystem::path const& cfg_file_path)
{
	auto package_folder = cfg_file_path.parent_path();
	auto parse_res = toml::parse_file(cfg_file_path.generic_string());
	if (!parse_res) {
		spdlog::error("Failed to parse valet config file: {}",
			      cfg_file_path.generic_string());
		return std::nullopt;
	}
	toml::table const& cfg_tbl = parse_res;
	auto const& package_toml = cfg_tbl["package"];
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

std::optional<PackageType> get_package_type(const std::string& type)
{
	if (type == "bin")
		return PackageType::Application;
	else if (type == "lib")
		return PackageType::StaticLibrary;
	else if (type == "dylib")
		return PackageType::SharedLibrary;
	else
		return std::nullopt;
}

} // namespace valet
