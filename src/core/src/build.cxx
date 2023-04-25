#include "valet/build.hxx"

// valet
#include "valet/package.hxx"
#include "string_utils.hxx"
#include "valet/stopwatch.hxx"
#include "platform.hxx"
#include "valet/thread_utils.hxx"

// stl
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <regex>

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
	auto build_plan = BuildPlan::make(*package_graph, params.compile_options, build_folder);
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

bool run_target_by_name(BuildPlan const& plan, std::string const& name, bool release)
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
	int ret = std::system(exe_path_str.c_str());
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
	for (auto const& target : params.targets) {
		if (!run_target_by_name(build_plan, target, params.build.compile_options.release))
			return false;
	}
	return true;
}

int execute(Command const& command)
{
	util::Stopwatch stopwatch;
	spdlog::trace("Executing: {}", command.cmd);
	auto ret = std::system(command.cmd.c_str());
	if (ret) {
		spdlog::error("Command failed with return code {}", ret);
	}
	spdlog::trace("Command took {}", stopwatch.elapsed_str());
	return ret;
}

void collect_source_files(std::filesystem::path const& folder,
			  std::vector<std::filesystem::path>& out)
{
	static const std::unordered_set<std::string> source_file_extensions = {".cxx", ".cpp",
									       ".c++", ".c"};
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
	enum Type { ObjectFile, Dependency };
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
			if (!std::filesystem::exists(output_dep_entry.path()))
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
					 CompileOptions const& opts,
					 std::filesystem::path const& build_folder)
{
	auto sorted_opt = package_graph.sorted();
	if (!sorted_opt || sorted_opt->empty())
		return std::nullopt;
	auto const& sorted = *sorted_opt;
	BuildPlan plan;
	plan.thread_pool = std::make_shared<util::ThreadPool>(platform::get_cpu_count() / 3);
	plan.package_graph = package_graph;
	for (auto const& package : sorted) {
		std::vector<std::filesystem::path> source_files;
		std::filesystem::path source_folder = package.folder / "src";
		if (!std::filesystem::exists(source_folder)) {
			spdlog::error(
			    "Package {} must include a 'src' folder that contains its source files",
			    package.id);
			return std::nullopt;
		}
		collect_source_files(source_folder, source_files);
		std::vector<CompileCommand> compile_commands;
		auto package_build_folder = build_folder / package.id;
		for (auto const& source_file : source_files) {
			auto cc = CompileCommand::make(
			    source_file, package,
			    /* Should 'public_includes' have transitive relation? */
			    package_graph.all_deps(package), opts, package_build_folder);
			compile_commands.push_back(cc);
		}
		plan.group(package, compile_commands, build_folder);
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
	auto lc =
	    LinkCommand::make(obj_files, package, package_graph.all_deps(package), build_folder);
	link_commands.push_back(lc);
}

bool BuildPlan::execute()
{
	util::Stopwatch sw;
	std::atomic_bool success = true;
	for (size_t i = 0; i < compile_commands.size(); ++i) {
		thread_pool->enqueue([&success, &cc = compile_commands, i] {
			auto const& cmd = cc[i];
			auto output_folder = cmd.obj_file.parent_path();
			if (!std::filesystem::exists(output_folder))
				std::filesystem::create_directories(output_folder);
			spdlog::info("Compiling ({}/{}) {}", i, cc.size(),
				     cmd.source_file.generic_string());
			auto ret = ::valet::execute(cmd);
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
		spdlog::info("Linking ({}/{}) {}", i, link_commands.size(),
			     cmd.binary_path.generic_string());
		auto ret = ::valet::execute(cmd);
		success.store(ret == 0 && success.load());
	}
	thread_pool->wait();
	double link_time = sw.elapsed() - compile_time;
	spdlog::debug("Compilation took {}", util::Stopwatch::elapsed_str(compile_time));
	spdlog::debug("Linking took {}", util::Stopwatch::elapsed_str(link_time));
	return success;
}

CompileCommand CompileCommand::make(std::filesystem::path const& source_file,
				    Package const& package,
				    std::vector<Package> const& dependencies,
				    CompileOptions const& opts,
				    std::filesystem::path const& output_folder)
{
	std::filesystem::path obj_file = output_folder / (source_file.filename().string() + ".o");
	std::stringstream cmd;
	cmd << "clang++" // TODO: CompilationSettings
	    << " -Wall"	 // TODO: CompilationOptions
	    << " -MD"
	    << " -c " << source_file.generic_string()
	    << " -std=" << package.std // TODO: ABI compatibility warnings
	    << " -o " << obj_file.generic_string();
	if (package.type == Package::Type::SharedLibrary) {
#ifdef _WIN32
		auto upper = util::to_upper(package.name);
		cmd << " -D" << upper << "_SHARED"
		    << " -D" << upper << "_EXPORTS"; // Whaat? Really?
#endif
	}
	if (opts.release)
		cmd << " -O3";
	else {
		cmd << " -g -O0";
#ifdef _WIN32
		cmd << " -gcodeview";
#endif
	}
	for (auto const& opt : package.compile_options) {
		cmd << " " << opt;
	}
	for (auto const& incl : package.includes) {
		cmd << " -I" << incl.generic_string();
	}
	for (auto const& dep : dependencies) {
		for (auto const& public_incl : dep.public_includes) {
			spdlog::trace("Adding public include {} of the dependency {} to package {}",
				      public_incl.generic_string(), dep.id, package.id);
			cmd << " -I" << public_incl;
		}
	}
	CompileCommand command;
	command.obj_file = obj_file;
	command.source_file = source_file;
	command.cmd = cmd.str();
	return command;
}

LinkCommand LinkCommand::make(std::vector<std::filesystem::path> const& obj_files,
			      Package const& package, std::vector<Package> const& dependencies,
			      std::filesystem::path const& build_folder)
{
	auto output_file_path = build_folder / package.id / package.name;
	auto output_file_path_str = output_file_path.generic_string();
	std::stringstream cmd;
	switch (package.type) {
	case Package::Type::Application:
	case Package::Type::SharedLibrary: {
		cmd << "clang++"; // TODO: CompilationSettings
		if (package.type == Package::Type::SharedLibrary)
			cmd << " -shared";
		for (auto const& o : obj_files) {
			cmd << " " << o;
		}
		std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
		    copy_dylib_deps;
		for (auto const& dep : dependencies) {
			auto dep_bin_path_no_ext = build_folder / dep.id / dep.name;
			auto dep_bin_path_str = dep_bin_path_no_ext.generic_string();
			if (dep.type == Package::Type::SharedLibrary) {
				spdlog::error("Sorry, but valet does not support linking against a "
					      "shared library yet. Offending package: {}",
					      dep.id);
				exit(1);
			}
			dep_bin_path_str += dep.target_ext();
			cmd << " " << dep_bin_path_str;
		}
		cmd << " -o " << output_file_path_str + package.target_ext();
		if (package.type == Package::Type::SharedLibrary) {
#if defined(_WIN32)
#elif defined(__APPLE__)
			cmd << " -Wl,-undefined,dynamic_lookup";
#elif defined(__linux__)
			auto soname = output_file_path.filename().generic_string();
			cmd << " -Wl,-soname," << soname;
#endif
		}
		break;
	}
	case Package::Type::StaticLibrary: {
		cmd << platform::static_link_command_prefix(output_file_path_str +
							    package.target_ext());
		for (auto const& o : obj_files) {
			cmd << " " << o.generic_string();
		}
		break;
	}
	}
	LinkCommand command;
	command.cmd = cmd.str();
	command.obj_files = obj_files;
	command.binary_path = output_file_path;
	return command;
}

bool BuildPlan::export_compile_commands(std::filesystem::path const& out) const
{
	std::stringstream ss;
	ss << "[\n";
	for (size_t i = 0; i < compile_commands.size(); ++i) {
		auto const& cc = compile_commands[i];
		std::string esc = std::regex_replace(cc.cmd, std::regex("\""), "\\\"");
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
	for (auto it = link_commands.begin(); it != link_commands.end();) {
		auto const& cmd = *it;
		bool should_link = false;
		for (auto const& obj : cmd.obj_files) {
			if (std::find_if(compile_commands.begin(), compile_commands.end(),
					 [&obj](CompileCommand const& cc) {
						 return cc.obj_file == obj;
					 }) != compile_commands.end()) {
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
