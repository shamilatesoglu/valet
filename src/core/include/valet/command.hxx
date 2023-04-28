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
};

struct Command {
	std::string cmd;
};

struct CompileCommand : Command {
	std::filesystem::path source_file;
	std::filesystem::path obj_file;
	static CompileCommand make(std::filesystem::path const& source_file, Package const& package,
				   std::vector<Package> const& dependencies,
				   CompileOptions const& opts,
				   std::filesystem::path const& output_folder);
};

struct LinkCommand : Command {
	std::vector<std::filesystem::path> obj_files;
	std::filesystem::path binary_path;
	static LinkCommand make(std::vector<std::filesystem::path> const& obj_files,
				Package const& package, std::vector<Package> const& dependencies,
				std::filesystem::path const& output_folder);
};

int execute(Command const& command);

} // namespace valet
