#include "valet/build.hxx"

// std
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <regex>

// valet
#include "valet/package.hxx"
#include "valet/string_utils.hxx"
#include "valet/platform.hxx"

namespace valet
{

int execute(Command const& command)
{
	spdlog::debug("Executing: {}", command.cmd);
	auto ret = std::system(command.cmd.c_str());
	if (ret) {
		spdlog::debug("Command failed with return code {}", ret);
	}
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
	}
	else {
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
			}
			else {
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

std::optional<BuildPlan> make_build_plan(DependencyGraph<Package> const& package_graph,
					 CompileOptions const& opts,
					 std::filesystem::path const& build_folder)
{
	auto sorted_opt = package_graph.sorted();
	if (!sorted_opt)
		return std::nullopt;
	auto const& sorted = *sorted_opt;
	BuildPlan plan(package_graph);
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
			auto cc = make_compile_command(
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
	if (package.type == PackageType::Application) {
		executable_targets[package.name] = package;
	}
	compile_commands.insert(compile_commands.begin(), package_cc.begin(), package_cc.end());
	std::vector<std::filesystem::path> obj_files;
	for (auto const& cc : package_cc) {
		obj_files.push_back(cc.obj_file);
	}
	auto lc =
	    make_link_command(obj_files, package, package_graph.all_deps(package), build_folder);
	link_commands.push_back(lc);
}

bool BuildPlan::execute_plan()
{
	optimize_plan();
	bool success = true;
	for (auto const& cmd : compile_commands) {
		auto output_folder = cmd.obj_file.parent_path();
		if (!std::filesystem::exists(output_folder))
			std::filesystem::create_directories(output_folder);
		spdlog::info("Compiling {}", cmd.source_file.generic_string());
		success &= ::valet::execute(cmd) == 0;
	}
	for (auto const& cmd : link_commands) {
		auto output_folder = cmd.binary_path.parent_path();
		if (!std::filesystem::exists(output_folder))
			std::filesystem::create_directories(output_folder);
		spdlog::info("Linking {}", cmd.binary_path.generic_string());
		success &= ::valet::execute(cmd) == 0;
	}
	return success;
}

CompileCommand make_compile_command(std::filesystem::path const& source_file,
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
	if (opts.release)
		cmd << " -O3";
	else {
		cmd << " -g -O0";
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

LinkCommand make_link_command(std::vector<std::filesystem::path> const& obj_files,
			      Package const& package, std::vector<Package> const& dependencies,
			      std::filesystem::path const& build_folder)
{
	auto output_file_path = build_folder / package.id / package.name;
	std::stringstream cmd;
	switch (package.type) {
	case Application: {
		cmd << "clang++"; // TODO: CompilationSettings
		for (auto const& o : obj_files) {
			cmd << " " << o;
		}
		for (auto const& dep : dependencies) {
			auto dep_bin_path = build_folder / dep.id / (dep.name + ".a");
			cmd << " " << dep_bin_path.generic_string();
		}
		auto output_file_path_str = output_file_path.generic_string();
		platform::add_executable_file_ext(output_file_path_str);
		cmd << " -o " << output_file_path_str;
		break;
	}
	case StaticLibrary: {
		cmd << platform::static_link_command_prefix(
		    output_file_path.generic_string() + ".a");
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

void BuildPlan::optimize_plan()
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
}

} // namespace valet
