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
	std::filesystem::path binary_path;
};

class BuildPlan
{
public:
	BuildPlan(DependencyGraph<Package> const& package_graph) : package_graph(package_graph) {}
	void group(Package const& package, std::vector<CompileCommand> const& package_cc,
		   std::filesystem::path const& build_folder);
	bool execute_plan() const;
	bool export_compile_commands(std::filesystem::path const& out) const;
	Package const* get_executable_target_by_name(std::string const& name) const;

private:
	DependencyGraph<Package> package_graph;
	std::vector<CompileCommand> compile_commands;
	std::vector<LinkCommand> link_commands;
	std::unordered_map<std::string, Package> executable_targets;
};

int execute(Command const& command);

void collect_source_files(std::filesystem::path const& folder,
			  std::vector<std::filesystem::path>& out);

std::optional<BuildPlan> make_build_plan(DependencyGraph<Package> const& package_graph,
					 std::filesystem::path const& build_folder);

CompileCommand make_compile_command(std::filesystem::path const& source_file,
				    Package const& package,
				    std::vector<Package> const& dependencies,
				    std::filesystem::path const& output_folder);

LinkCommand make_link_command(std::vector<std::filesystem::path> const& obj_files,
			      Package const& package, std::vector<Package> const& dependencies,
			      std::filesystem::path const& output_folder);

} // namespace autob