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

// autob
#include "graph.hxx"

namespace autob
{

enum PackageType {
	Application,
	StaticLibrary,
};

struct DependencyInfo {
	std::string name;
	// Folder might also be the download location of a remote package
	std::filesystem::path folder;
};

struct Package : Identifiable {
	std::string name;
	std::string version;
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
template <>
struct hash<autob::Identifiable> {
	size_t operator()(const autob::Identifiable& key) const
	{
		return ::std::hash<std::string>()(key.id);
	}
};

template <>
struct hash<autob::Package> {
	size_t operator()(const autob::Package& key) const
	{
		return ::std::hash<std::string>()(key.id);
	}
};

} // namespace std

namespace autob
{

std::optional<Package> find_package(std::filesystem::path const& folder);
Package parse_package_cfg(std::filesystem::path const& cfg_file_path);
std::optional<DependencyGraph<Package>>
make_package_graph(std::filesystem::path const& project_folder);

} // namespace autob
