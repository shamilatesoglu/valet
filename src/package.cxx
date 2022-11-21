#include "package.hxx"

// std
#include <stack>
#include <unordered_set>

// external
#include <tomlplusplus/toml.hpp>
#include <spdlog/spdlog.h>

namespace autob
{

std::optional<PackageGraph> make_package_graph(std::filesystem::path const& project_folder)
{
	auto package = find_package(project_folder); // TODO: Handle parse errors
	if (!package) {
		return std::nullopt;
	}
	PackageGraph package_graph;

	std::stack<std::pair<Package, DependencyInfo>> to_be_resolved;
	for (auto const& dep_info : package->dependencies) {
		to_be_resolved.push({*package, dep_info});
	}

	while (!to_be_resolved.empty()) {
		auto pair = to_be_resolved.top();
		to_be_resolved.pop();
		auto const& cur_dep_info = pair.second;
		auto const& cur_package = pair.first;
		// TODO: For the package server, this needs to fetch the details of
		// the package from the internet instead of parsing from path
		auto resolved = find_package(cur_dep_info.path);
		if (!resolved) {
			spdlog::error("Unable to resolve dependency {} {}", cur_dep_info.name,
				      cur_dep_info.version);
			return std::nullopt;
		}
		package_graph.depend(cur_package, resolved.value());
		for (auto const& dep_info : resolved->dependencies) {
			to_be_resolved.push({*resolved, dep_info});
		}
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
		dep_info.name = entry.first.str();
		dep_info.path =
		    std::filesystem::canonical(package_folder / entry.second.as_string()->get());
		package.dependencies.push_back(dep_info);
	}
	return package;
}

bool PackageGraph::depend(Package const& dependent, Package const& dependence)
{
	depgraph[dependent].push_back(dependence);
	return true;
}

bool PackageGraph::is_valid() const
{
	// TODO: Check cycle
	return true;
}

std::optional<std::vector<Package>> PackageGraph::sorted() const
{
	class TopologicalSorter
	{
	public:
		TopologicalSorter(std::unordered_map<Package, std::vector<Package>> const& depgraph)
		    : depgraph(depgraph)
		{
			for (auto const& [package, deplist] : depgraph)
				unmarked.insert(package);
		}

		bool sort()
		{
			while (perm_marked.empty()) {
				auto some = *unmarked.begin();
				if (!visit(some)) {
					return false;
				}
			}
			return true;
		}

		std::vector<Package> sorted;

	private:
		bool visit(Package const& node)
		{
			if (perm_marked.contains(node))
				return true;
			if (temp_marked.contains(node)) {
				spdlog::error("Dependency graph contains a cycle!");
				return false;
			}
			temp_marked.insert(node);
			unmarked.erase(node);
			for (auto const& [package, deplist] : depgraph) {
				for (auto const& dep : deplist) {
					if (!visit(dep))
						return false;
				}
			}
			temp_marked.erase(node);
			perm_marked.insert(node);
			sorted.push_back(node);
			return true;
		}

		std::unordered_set<Package> perm_marked;
		std::unordered_set<Package> temp_marked;
		std::unordered_set<Package> unmarked;
		std::unordered_map<Package, std::vector<Package>> const& depgraph;
	};

	TopologicalSorter sorter(depgraph);
	if (!sorter.sort())
		return std::nullopt;

	return sorter.sorted;
}

std::vector<Package> const& PackageGraph::immediate_deps(Package const& package) const
{
	auto const& it = depgraph.find(package);
	return it->second;
}

std::vector<Package> PackageGraph::all_deps(const Package& package) const
{
	std::vector<Package> all_deps;
	std::stack<Package> stack;
	for (auto const& dep : immediate_deps(package)) {
		stack.push(dep);
	}
	while (!stack.empty()) {
		auto const& cur = stack.top();
		all_deps.push_back(cur);
		stack.pop();
		for (auto const& dep : immediate_deps(cur)) {
			stack.push(dep);
		}
	}
	return all_deps;
}

} // namespace autob