#pragma once

// std
#include <filesystem>
#include <vector>

// external
#include <spdlog/spdlog.h>

// autob
#include "package.hxx"

namespace autob
{

struct Command {
	std::string cmd;
};

struct CompileCommand : Command {
	std::filesystem::path source_file;
	std::filesystem::path obj_file;
};

struct LinkCommand : Command {
	std::vector<std::filesystem::path> obj_files;
	std::optional<std::filesystem::path> binary;
};

class BuildPlan
{
public:
	BuildPlan(PackageGraph const& package_graph) : package_graph(package_graph) {}
	void add_package(Package const& package, std::vector<CompileCommand> const& package_cc);

private:
	PackageGraph package_graph;
	std::vector<CompileCommand> compile_commands;
	std::vector<LinkCommand> link_commands;
};

int execute(Command const& command);

void collect_source_files(std::filesystem::path const& folder,
			  std::vector<std::filesystem::path>& out);

std::optional<BuildPlan> make_build_plan(PackageGraph const& package_graph,
					 std::filesystem::path const& build_folder);

CompileCommand make_compile_command(std::filesystem::path const& source_file,
				    Package const& package,
				    std::vector<Package> const& dependencies,
				    std::filesystem::path const& output_folder);

LinkCommand make_link_command(std::vector<std::filesystem::path> const& obj_files,
			      Package const& package, std::vector<Package> const& dependencies,
			      std::filesystem::path const& output_folder);

} // namespace autob