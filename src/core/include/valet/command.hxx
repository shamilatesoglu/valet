#pragma once

// valet
#include "valet/package.hxx"

// stl
#include <string>
#include <filesystem>
#include <vector>

namespace valet
{

struct CompileOptions {
	bool release = false;
	std::vector<std::string> additional_options;
	uint32_t mp_count = 0;
};

struct Command {
	virtual std::string string() const = 0;
	operator std::string() const { return string(); }
};

struct CompileCommand : Command {
	Package package;
	std::filesystem::path source_file;
	std::filesystem::path obj_file;
	std::unordered_set<Package> dependencies;
	CompileOptions opts;
	std::filesystem::path output_folder;
	std::string string() const override;
	CompileCommand(Package const& package, std::filesystem::path const& source_file,
		       std::unordered_set<Package> const& dependencies, CompileOptions const& opts,
		       std::filesystem::path const& output_folder);
	virtual ~CompileCommand() = default;
};

struct LinkCommand : Command {
	Package package;
	std::vector<std::filesystem::path> obj_files;
	std::unordered_set<Package> dependencies;
	std::filesystem::path binary_path;
	std::string string() const override;
	LinkCommand(Package const& package, std::vector<std::filesystem::path> const& obj_files,
		    std::unordered_set<Package> const& dependencies,
		    std::filesystem::path const& output_folder);
	virtual ~LinkCommand() = default;
};

int execute(std::string const& command,
	    std::optional<std::filesystem::path> const& working_dir = std::nullopt);

} // namespace valet
