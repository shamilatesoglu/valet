#pragma once

// std
#include <optional>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace autob
{

enum PackageType {
	Application,
	StaticLibrary,
};

struct DependencyInfo {
	std::string name;
	std::filesystem::path path;
	std::string version;
};

struct Package {
	std::string name;
	std::string version;
	std::string std;
	PackageType type;
	std::vector<std::filesystem::path> public_includes;
	std::vector<std::filesystem::path> includes;
	std::vector<DependencyInfo> dependencies;
	std::filesystem::path folder;
	inline std::string id() const { return name + "=" + version; }
	bool operator==(Package const& rhs) const { return id() == rhs.id(); }
};

} // namespace autob

namespace std
{
template <> struct hash<autob::Package> {
	size_t operator()(const autob::Package& key) const
	{
		return ::std::hash<std::string>()(key.id());
	}
};

} // namespace std

namespace autob
{

/**
 * @brief Dependency graph for packages and their dependencies
 */
class PackageGraph
{
public:
	bool depend(Package const& dependent, Package const& dependence);
	bool is_valid() const;
	std::optional<std::vector<Package>> sorted() const;
	std::vector<Package> const& immediate_deps(Package const& package) const;
	std::vector<Package> all_deps(Package const& package) const;

private:
	// Package -> Dependencies
	std::unordered_map<Package, std::vector<Package>> depgraph;
};

std::optional<std::filesystem::path> find_package_path(std::filesystem::path const& folder);
std::optional<Package> find_package(std::filesystem::path const& folder);
Package parse_package_cfg(std::filesystem::path const& cfg_file_path);
std::optional<PackageGraph> make_package_graph(std::filesystem::path const& project_folder);

} // namespace autob
