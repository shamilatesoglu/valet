#pragma once

// valet
#include "valet/package.hxx"
#include "valet/command.hxx"
#include "valet/thread_utils.hxx"

// external
#include <spdlog/spdlog.h>

// stl
#include <filesystem>
#include <vector>

namespace valet
{

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
	std::optional<std::string> arguments;
};

struct BuildStats {
	double total_time_s = 0.0;
	double package_resolution_time_s = 0.0;
	double compilation_time_s = 0.0;
	double link_time_s = 0.0;
	std::vector<std::pair<std::filesystem::path, double>> compilation_times;
	std::vector<std::pair<std::filesystem::path, double>> link_times;
	std::string to_string() const;
};

bool build(BuildParams const& params, class BuildPlan* out = nullptr);

bool run(RunParams& params);

class BuildPlan
{
public:
	static std::optional<BuildPlan> make(DependencyGraph<Package> const& package_graph,
					     CompileOptions const& opts,
					     bool collect_stats,
					     std::filesystem::path const& build_folder);
	void group(Package const& package, std::vector<CompileCommand> const& package_cc,
		   std::filesystem::path const& build_folder);
	bool execute();
	void optimize();
	bool export_compile_commands(std::filesystem::path const& out) const;
	Package const* get_executable_target_by_name(std::string const& name) const;
	Package root;

protected:
	DependencyGraph<Package> package_graph;
	std::vector<CompileCommand> compile_commands;
	std::vector<LinkCommand> link_commands;
	std::unordered_map<std::string, Package> executable_targets;
	std::shared_ptr<util::ThreadPool> thread_pool;

	bool collect_stats = false;
	BuildStats stats;
};

void collect_source_files(std::filesystem::path const& folder,
			  std::vector<std::filesystem::path>& out);

std::filesystem::path get_build_folder(std::filesystem::path const& project_folder, bool release);

} // namespace valet