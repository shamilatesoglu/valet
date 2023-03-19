#pragma once

// stl
#include <filesystem>
#include <vector>

// external
#include <spdlog/spdlog.h>

// valet
#include "valet/package.hxx"

namespace valet
{

struct CompileOptions {
	bool release = false;
	std::vector<std::string> additional_options;
};

struct BuildParams {
	std::filesystem::path project_folder;
	CompileOptions compile_options;
	bool dry_run = false;
	bool clean = false;
	bool export_compile_commands = false;
	bool collect_stats = false; // TODO.
};

struct RunParams {
	BuildParams build;
	std::vector<std::string> targets; // If emtpy, run the root package if it is an executable.
};

bool build(BuildParams const& params, class BuildPlan* out = nullptr);

bool run(RunParams& params);

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

class BuildPlan
{
public:
	static std::optional<BuildPlan> make(DependencyGraph<Package> const& package_graph,
					     CompileOptions const& opts,
					     std::filesystem::path const& build_folder);
	void group(Package const& package, std::vector<CompileCommand> const& package_cc,
		   std::filesystem::path const& build_folder);
	bool execute_plan();
	bool export_compile_commands(std::filesystem::path const& out) const;
	Package const* get_executable_target_by_name(std::string const& name) const;

	Package root;

protected:
	void optimize_plan();

	DependencyGraph<Package> package_graph;
	std::vector<CompileCommand> compile_commands;
	std::vector<LinkCommand> link_commands;
	std::unordered_map<std::string, Package> executable_targets;
};

int execute(Command const& command);

void collect_source_files(std::filesystem::path const& folder,
			  std::vector<std::filesystem::path>& out);

std::filesystem::path get_build_folder(std::filesystem::path const& project_folder, bool release);

} // namespace valet