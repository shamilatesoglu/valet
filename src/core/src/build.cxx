#include "valet/build.hxx"

// valet
#include "valet/package.hxx"
#include "string_utils.hxx"
#include "valet/stopwatch.hxx"
#include "valet/command.hxx"
#include "platform.hxx"
#include "valet/thread_utils.hxx"

// external
#include <tsl/ordered_set.h>

// stl
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <iostream>

namespace valet
{

std::filesystem::path get_build_folder(std::filesystem::path const& project_folder, bool release)
{
	auto build_folder = project_folder / "build" / (release ? "release" : "debug");
	if (!std::filesystem::exists(build_folder))
		std::filesystem::create_directories(build_folder);
	return build_folder;
}

bool build(BuildParams const& params, BuildPlan* out)
{
	auto build_folder = get_build_folder(params.project_folder, params.compile_options.release);
	if (params.clean) {
		spdlog::info("Cleaning build");
		std::filesystem::remove_all(build_folder);
	}
	util::Stopwatch sw;
	auto package_graph = make_package_graph(params.project_folder);
	if (!package_graph) {
		spdlog::error("Unable to parse package graph");
		return false;
	}
	if (package_graph->empty()) {
		spdlog::error("No package found under {}", params.project_folder.generic_string());
		return false;
	}
	spdlog::debug("Package graph parsed in {}", sw.elapsed_str());
	sw.reset();
	auto build_plan = BuildPlan::make(*package_graph, params.compile_options,
					  params.collect_stats, build_folder);
	if (!build_plan) {
		spdlog::error("Unable to generate build plan");
		return false;
	}
	build_plan->root = *find_package(params.project_folder); // Copies, copies everywhere.
	spdlog::debug("Build plan generated in {}", sw.elapsed_str());
	if (params.export_compile_commands) {
		build_plan->export_compile_commands(params.project_folder);
	}
	sw.reset();
	if (params.dry_run) {
		return true;
	}
	build_plan->optimize();
	spdlog::debug("Build plan optimized in {}", sw.elapsed_str());
	bool success = build_plan->execute();
	if (out)
		*out = *build_plan;
	return success;
}

bool run_target_by_name(BuildPlan const& plan, std::string const& name, std::string const& args, bool release)
{
	auto* executable = plan.get_executable_target_by_name(name);
	if (!executable) {
		spdlog::error("No such executable target: {}", name);
		return false;
	}
	auto exe_build_folder = get_build_folder(executable->folder, release);
	std::filesystem::path exe_path = exe_build_folder / executable->id / executable->name;
	auto exe_path_str = exe_path.generic_string() + executable->target_ext();
	spdlog::info("Running target {}", exe_path_str);
	valet::platform::escape_cli_command(exe_path_str);
	exe_path_str = "\"" + exe_path_str + "\"";
	int ret = std::system((exe_path_str + " " + args).c_str());
	if (ret) {
		spdlog::error("Execution failed with return code {}", ret);
		return false;
	}
	return true;
}

bool run(RunParams& params)
{
	BuildPlan build_plan;
	if (!build(params.build, &build_plan)) {
		spdlog::error("Build failed");
		return false;
	}
	if (params.targets.empty()) {
		auto root_package = find_package(params.build.project_folder);
		if (!root_package) {
			spdlog::error("Unable to find root package");
			return false;
		}
		if (root_package->type != Package::Type::Application) {
			spdlog::error("Root package {} is not an executable", root_package->name);
			return false;
		}
		params.targets.emplace_back(root_package->name);
	}
	if (params.arguments && params.targets.size() > 1)
		spdlog::warn("Multiple targets will be run with same arguments.");
	for (auto const& target : params.targets) {
		if (!run_target_by_name(build_plan, target, params.arguments.value_or(""), params.build.compile_options.release))
			return false;
	}
	return true;
}

void collect_source_files(std::filesystem::path const& folder,
			  std::vector<std::filesystem::path>& out)
{
	static const std::unordered_set<std::string> source_file_extensions = {".cxx", ".cpp",
									       ".cc", ".c++", ".c"};
	for (auto const& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.is_directory()) {
			if (auto nested = find_package(entry.path()))
				// TODO: Figure out how to do nested packages.
				continue;
			collect_source_files(entry.path(), out);
		} else if (source_file_extensions.contains(entry.path().extension().string())) {
			auto source_file = std::filesystem::canonical(entry.path());
			out.push_back(source_file);
			spdlog::debug("Found source file: {}", source_file.generic_string());
		}
	}
}

struct DepFileEntry : Identifiable {
	enum Type {
		ObjectFile,
		Dependency
	};
	DepFileEntry(std::filesystem::path const& path, Type type) : type(type)
	{
		id = std::filesystem::weakly_canonical(path).generic_string();
		platform::sanitize_path(id);
	}
	Type type;
	std::filesystem::path path() const { return id; }
};

} // namespace valet

namespace std
{

template <>
struct hash<valet::DepFileEntry> {
	size_t operator()(const valet::DepFileEntry& key) const
	{
		return ::std::hash<std::string>()(key.id);
	}
};

} // namespace std

namespace valet
{

void collect_source_deps(std::filesystem::path const& depfile_path,
			 DependencyGraph<DepFileEntry>& out)
{
	// TODO: Optimize
	if (!std::filesystem::exists(depfile_path))
		return;
	std::ifstream depfile_stream(depfile_path);
	std::stringstream depfile_str;
	depfile_str << depfile_stream.rdbuf();
	depfile_stream.close();

	std::vector<std::string> lines = util::split(depfile_str.str(), "\n");
	for (auto& line : lines) {
		util::strip(line);
		if (line.ends_with("\\"))
			line.erase(line.end() - 1);
		util::strip(line);
	}

	// TODO: Pretty nasty. Might need to be more robust.
	auto& first_line = lines[0];
	// If the first line contains a dep after the last colon, take it and insert after lines[0].
	auto colon_pos = first_line.find_last_of(": ");
	if (colon_pos != first_line.size() - 1) {
		auto dep_str = first_line.substr(colon_pos - 1);
		util::strip(dep_str);
		lines.insert(lines.begin() + 1, dep_str);
		// Remove the colon with the leftover characters from the first line.
		first_line = first_line.substr(0, colon_pos - 3);
	} else {
		first_line = first_line.substr(0, colon_pos);
	}

	DepFileEntry o_entry(first_line, DepFileEntry::ObjectFile);
	out.add(o_entry);

	for (size_t i = 0; i < lines.size(); ++i) {
		auto const& dep_str = lines[i];
		if (dep_str.empty())
			continue;
		DepFileEntry d_entry(dep_str, DepFileEntry::Dependency);
		out.add(d_entry);
		out.depend(o_entry, d_entry);
	}
}

bool has_modified_deps(DepFileEntry const& output_dep_entry,
		       std::unordered_set<DepFileEntry> const& deps)
{
	for (auto const& dep : deps) {
		try {
			if (!std::filesystem::exists(output_dep_entry.path()) ||
			    !std::filesystem::exists(dep.path()))
				return true;
			auto tmout = std::filesystem::last_write_time(output_dep_entry.path());
			auto tmdep = std::filesystem::last_write_time(dep.path());
			if (tmdep > tmout) {
				spdlog::trace("Dependency {} is newer than output {}, will compile",
					      dep.path().generic_string(),
					      output_dep_entry.path().generic_string());
				return true;
			} else {
				spdlog::trace("Dependency {} is older than output {}",
					      dep.path().generic_string(),
					      output_dep_entry.path().generic_string());
			}
		} catch (std::runtime_error const& err) {
			spdlog::error(err.what());
			throw err;
		}
	}
	return false;
}

std::optional<BuildPlan> BuildPlan::make(DependencyGraph<Package> const& package_graph,
					 CompileOptions const& opts, bool collect_stats,
					 std::filesystem::path const& build_folder)
{
	auto sorted_opt = package_graph.sorted();
	if (!sorted_opt || sorted_opt->empty())
		return std::nullopt;
	auto const& sorted = *sorted_opt;
	BuildPlan plan;
	plan.collect_stats = collect_stats;
	auto nth = opts.mp_count;
	if (nth == 0) {
		auto rec = std::max(1u, platform::get_cpu_count() / 2) - 1;
		nth = std::max(1u, rec);
	}
	spdlog::debug("Using {} threads to build", nth);
	plan.thread_pool = std::make_shared<util::ThreadPool>(nth);
	plan.package_graph = package_graph;
	for (auto const& package : sorted) {
		if (package.type == Package::Type::HeaderOnly)
			continue;
		std::vector<std::filesystem::path> source_files;
		std::filesystem::path source_folder = package.folder / "src";
		if (!std::filesystem::exists(source_folder)) {
			spdlog::error(
			    "Package {} must include a 'src' folder that contains its source files",
			    package.id);
			return std::nullopt;
		}
		collect_source_files(source_folder, source_files);
		std::vector<CompileCommand> package_cc;
		auto package_build_folder = build_folder / package.id;
		for (auto const& source_file : source_files) {
			CompileCommand cc(package, source_file, package_graph.all_deps(package),
					  opts, package_build_folder);
			package_cc.push_back(cc);
		}
		plan.group(package, package_cc, build_folder);
		spdlog::debug("Package {} has {} source files", package.id, source_files.size());
	}
	return plan;
}

void BuildPlan::group(Package const& package, std::vector<CompileCommand> const& package_cc,
		      std::filesystem::path const& build_folder)
{
	if (package.type == Package::Type::Application) {
		executable_targets[package.name] = package;
	}
	compile_commands.insert(compile_commands.begin(), package_cc.begin(), package_cc.end());
	std::vector<std::filesystem::path> obj_files;
	for (auto const& cc : package_cc) {
		obj_files.push_back(cc.obj_file);
	}
	auto lc = LinkCommand(package, obj_files, package_graph.all_deps(package), build_folder);
	link_commands.push_back(lc);
}

bool BuildPlan::execute()
{
	static std::mutex stats_mutex;
	util::Stopwatch sw;
	std::atomic_bool success = true;
	for (size_t i = 0; i < compile_commands.size(); ++i) {
		thread_pool->enqueue([&success, &cc = compile_commands, i, this] {
			auto const& cmd = cc[i];
			auto output_folder = cmd.obj_file.parent_path();
			if (!std::filesystem::exists(output_folder))
				std::filesystem::create_directories(output_folder);
			spdlog::info("Compiling ({}/{}) {}", i + 1, cc.size(),
				     cmd.source_file.generic_string());
			util::Stopwatch csw;
			auto ret = ::valet::execute(cmd);
			double compilation_time = csw.elapsed();
			if (ret == 0 && collect_stats) {
				std::unique_lock lock(stats_mutex);
				stats.compilation_time_s += compilation_time;
				stats.compilation_times.emplace_back(
				    std::pair<std::filesystem::path, double>(cmd.source_file,
									     compilation_time));
			}
			success.store(ret == 0 && success.load());
		});
	}
	thread_pool->wait();
	double compile_time = sw.elapsed();
	for (size_t i = 0; i < link_commands.size(); ++i) {
		auto const& cmd = link_commands[i];
		auto output_folder = cmd.binary_path.parent_path();
		if (!std::filesystem::exists(output_folder))
			std::filesystem::create_directories(output_folder);
		spdlog::info("Linking ({}/{}) {}", i + 1, link_commands.size(),
			     cmd.binary_path.generic_string());
		util::Stopwatch lsw;
		auto ret = ::valet::execute(cmd);
		double linking_time = lsw.elapsed();
		if (ret == 0 && collect_stats) {
			std::unique_lock lock(stats_mutex);
			stats.link_time_s += linking_time;
			stats.link_times.emplace_back(std::pair<std::filesystem::path, double>(
			    cmd.binary_path, linking_time));
		}
		success.store(ret == 0 && success.load());
	}
	double link_time = sw.elapsed() - compile_time;
	spdlog::debug("Compilation took {}", util::Stopwatch::elapsed_str(compile_time));
	spdlog::debug("Linking took {}", util::Stopwatch::elapsed_str(link_time));
	if (collect_stats) {
		stats.total_time_s = sw.elapsed();
		std::cout << stats.to_string() << std::endl;
	}
	return success;
}

std::string BuildStats::to_string() const
{
	std::stringstream ss;
	ss << "\n";

	// Compilation times table
	ss << std::setw(40) << std::left << "Source File" << std::setw(30) << std::right
	   << "Compilation Time (s)"
	   << "\n";
	ss << std::setw(80) << std::setfill('-') << "\n";
	ss << std::setfill(' ');

	std::vector<std::pair<std::string, double>> compilation_times_formatted;
	for (auto&& [path, time] : compilation_times) {
		std::string filename = std::filesystem::path(path).filename().string();
		compilation_times_formatted.emplace_back(filename, time);
	}
	std::sort(
	    compilation_times_formatted.begin(), compilation_times_formatted.end(),
	    [](const std::pair<std::string, double>& lhs,
	       const std::pair<std::string, double>& rhs) { return lhs.second > rhs.second; });

	for (auto&& [filename, time] : compilation_times_formatted) {
		ss << std::setw(40) << std::left << filename << std::setw(30) << std::right
		   << std::fixed << std::setprecision(2) << time << "\n";
	}
	ss << "\n";

	// Link times table
	ss << std::setw(40) << std::left << "Binary" << std::setw(30) << std::right
	   << "Link Time (s)"
	   << "\n";
	ss << std::setw(80) << std::setfill('-') << "\n";
	ss << std::setfill(' ');

	std::vector<std::pair<std::string, double>> link_times_formatted;
	for (auto&& [path, time] : link_times) {
		std::string filename = std::filesystem::path(path).filename().string();
		link_times_formatted.emplace_back(filename, time);
	}
	std::sort(
	    link_times_formatted.begin(), link_times_formatted.end(),
	    [](const std::pair<std::string, double>& lhs,
	       const std::pair<std::string, double>& rhs) { return lhs.second > rhs.second; });

	for (auto&& [filename, time] : link_times_formatted) {
		ss << std::setw(40) << std::left << filename << std::setw(30) << std::right
		   << std::fixed << std::setprecision(2) << time << "\n";
	}

	ss << "\nTotal time: " << std::fixed << std::setprecision(2) << total_time_s << " s\n"
	   << "Package resolution time: " << std::fixed << std::setprecision(2)
	   << package_resolution_time_s << " s\n"
	   << "Compilation time: " << std::fixed << std::setprecision(2) << compilation_time_s
	   << " s\n"
	   << "Link time: " << std::fixed << std::setprecision(2) << link_time_s << " s\n";

	return ss.str();
}

bool BuildPlan::export_compile_commands(std::filesystem::path const& out) const
{
	std::stringstream ss;
	ss << "[\n";
	for (size_t i = 0; i < compile_commands.size(); ++i) {
		auto const& cc = compile_commands[i];
		std::string esc = std::regex_replace(cc.string(), std::regex("\""), "\\\"");
		// clang-format off
		ss << "{"
		   << "\n\t\"directory\": \"" << cc.source_file.parent_path().generic_string() << "\","
		   << "\n\t\"command\": \"" << esc << "\","
		   << "\n\t\"file\": \"" << cc.source_file.generic_string() << "\""
		   << "\n}";
		// clang-format on
		if (i != compile_commands.size() - 1) {
			ss << ",";
		}
		ss << "\n";
	}
	ss << "]\n";
	auto out_file_path = out / "compile_commands.json";
	spdlog::info("Exporting compile commands: {}", out_file_path.generic_string());
	std::ofstream of(out_file_path);
	of << ss.str();
	of.close();
	return true;
}

Package const* BuildPlan::get_executable_target_by_name(std::string const& name) const
{
	auto it = executable_targets.find(name);
	if (it == executable_targets.end()) {
		spdlog::trace("No such executable target: {}", name);
		spdlog::trace("\tExecutable targets are: ");
		for (auto [name, target] : executable_targets) {
			spdlog::trace("\t\t{}", target.id);
		}
		return nullptr;
	}
	return &it->second;
}

void BuildPlan::optimize()
{
	spdlog::debug("Optimizing build plan");
	// Actually, dependency graph is not needed (and consequently, one pass solution would be
	// sufficient).
	DependencyGraph<DepFileEntry> depgraph;
	for (auto const& cc : compile_commands) {
		std::filesystem::path depfile_path =
		    cc.obj_file.parent_path() / (cc.obj_file.stem().generic_string() + ".d");
		collect_source_deps(depfile_path, depgraph);
	}
	for (auto it = compile_commands.begin(); it != compile_commands.end();) {
		auto const& cmd = *it;
		DepFileEntry entry(cmd.obj_file, DepFileEntry::ObjectFile);
		bool should_compile = true;
		if (depgraph.has(entry)) {
			if (!has_modified_deps(entry, depgraph.immediate_deps(entry)))
				should_compile = false;
			// List any other decisions here
		} else {
			spdlog::trace("No depfile found for {}", cmd.obj_file.generic_string());
		}
		if (!should_compile) {
			spdlog::trace("Skipped compiling {}", cmd.source_file.generic_string());
			it = compile_commands.erase(it);
		} else {
			++it;
		}
	}
	// Now, optimize link commands
	// TODO: If a static library is not need to be compiled,
	//       do not link it even if it is a dependency to a static library that needs to be
	//       compiled
	tsl::ordered_set<Package> packages_to_be_compiled;
	for (auto const& cc : compile_commands) {
		packages_to_be_compiled.insert(cc.package);
	}
	tsl::ordered_set<Package> to_be_linked;
	for (auto const& package : packages_to_be_compiled) {
		to_be_linked.insert(package);
		auto dependants = package_graph.all_dependants(package);
		spdlog::trace("Package {} is a dependency to {} packages", package.id,
			      dependants.size());
		for (auto const& dep : dependants) {
			to_be_linked.insert(dep);
		}
	}
	spdlog::trace("Will link {} packages", to_be_linked.size());
	for (auto it = link_commands.begin(); it != link_commands.end();) {
		auto const& cmd = *it;
		bool should_link = false;

		for (auto const& dep : to_be_linked) {
			if (dep == cmd.package) {
				should_link = true;
				break;
			}
		}

		if (!should_link) {
			spdlog::trace("Skipped linking {}", cmd.binary_path.generic_string());
			it = link_commands.erase(it);
		} else {
			++it;
		}
	}
}

} // namespace valet
