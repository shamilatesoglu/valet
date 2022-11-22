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

struct Identifiable {
	std::string name;
	std::string version;
	inline std::string id() const { return name + "=" + version; }
	bool operator==(Identifiable const& rhs) const { return id() == rhs.id(); }
};

struct DependencyInfo : Identifiable {
	std::filesystem::path path;
};

struct Package : Identifiable {
	std::string std;
	PackageType type;
	std::vector<std::filesystem::path> public_includes;
	std::vector<std::filesystem::path> includes;
	std::vector<DependencyInfo> dependencies;
	std::filesystem::path folder;
};

} // namespace autob

namespace std
{
template <> struct hash<autob::Identifiable> {
	size_t operator()(const autob::Identifiable& key) const
	{
		return ::std::hash<std::string>()(key.id());
	}
};

template <> struct hash<autob::Package> {
	size_t operator()(const autob::Package& key) const
	{
		return ::std::hash<std::string>()(key.id());
	}
};

template <> struct hash<autob::DependencyInfo> {
	size_t operator()(const autob::DependencyInfo& key) const
	{
		return ::std::hash<std::string>()(key.id());
	}
};

} // namespace std

namespace autob
{

/**
 * @brief Dependency graph for packages and their dependencies
 * TODO: A generic dependency graph is needed.
 */
class PackageGraph
{
public:
	void add_package(Package const& package);
	bool depend(Package const& dependent, Package const& dependence);
	bool is_valid() const;
	std::optional<std::vector<Package>> sorted() const;
	std::vector<Package> const& immediate_deps(Package const& package) const;
	std::vector<Package> all_deps(Package const& package) const;
	bool empty() const;
	Package const* get_package_by_id(std::string const& id) const;
	std::unordered_map<Package, std::vector<Package>>::const_iterator begin() const;
	std::unordered_map<Package, std::vector<Package>>::const_iterator end() const;
	size_t size() const;

private:
	// Package -> Dependencies
	std::unordered_map<Package, std::vector<Package>> depgraph;
};

std::optional<Package> find_package(std::filesystem::path const& folder);
Package parse_package_cfg(std::filesystem::path const& cfg_file_path);
std::optional<PackageGraph> make_package_graph(std::filesystem::path const& project_folder);

} // namespace autob
