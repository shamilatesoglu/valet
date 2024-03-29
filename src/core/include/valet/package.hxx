#pragma once

// valet
#include "valet/graph.hxx"

// stl
#include <optional>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace valet
{

struct DependencyInfo {
	std::string name;
	// Folder might also be the download location of a remote package
	std::filesystem::path folder;
};

struct Package : Identifiable {
	std::string name;
	std::string version;
	std::string std;
	enum class Type { Application, StaticLibrary, SharedLibrary, HeaderOnly };
	Type type;
	std::vector<std::filesystem::path> public_includes;
	std::vector<std::filesystem::path> includes;
	std::vector<std::string> compile_options;
	std::vector<DependencyInfo> dependencies;
	std::filesystem::path folder;
	std::filesystem::path target_path(bool release) const;
	std::string target_ext() const;
	static std::optional<Package> parse_from(std::filesystem::path const& manifest_fp);
};

} // namespace valet

namespace std
{

template <>
struct hash<valet::Package> {
	size_t operator()(const valet::Package& key) const
	{
		return ::std::hash<std::string>()(key.id);
	}
};

} // namespace std

namespace valet
{

std::optional<Package> find_package(std::filesystem::path const& folder);
std::optional<Package::Type> get_package_type(std::string const& type);
// TODO: std::optional<DependencyGraph<Package*>>
// Avoid unnecessary copies. Build the graph from existing package objects retained somewhere.
std::optional<DependencyGraph<Package>>
make_package_graph(std::filesystem::path const& project_folder);

} // namespace valet
