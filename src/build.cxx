#include "build.hxx"

// std
#include <unordered_set>
#include <sstream>

// autob
#include "package.hxx"

namespace autob
{

int execute(Command const& command)
{
	spdlog::trace("Executing: {}", command.cmd);
	return std::system(command.cmd.c_str());
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

std::optional<BuildPlan> make_build_plan(PackageGraph const& package_graph,
					 std::filesystem::path const& build_folder)
{
	auto sorted_opt = package_graph.sorted();
	if (!sorted_opt)
		return std::nullopt;
	auto const& sorted = *sorted_opt;
	BuildPlan plan(package_graph);
	for (auto const& package : sorted) {
		std::vector<std::filesystem::path> source_files;
		collect_source_files(package.folder, source_files);
		std::vector<CompileCommand> compile_commands;
		auto package_build_folder = build_folder / package.id();
		for (auto const& source_file : source_files) {
			auto cc = make_compile_command(
			    source_file, package,
			    /* Are 'public_includes' transitively related? */
			    package_graph.all_deps(package), package_build_folder);
			compile_commands.push_back(cc);
		}
		plan.add_package(package, compile_commands);
	}
	return plan;
}

void BuildPlan::add_package(Package const& package, std::vector<CompileCommand> const& package_cc)
{
	// This assumes package build folder is a flat
	// tree (every object file is in the same dir).
	auto package_build_folder = package_cc.front().obj_file.parent_path();
	compile_commands.insert(compile_commands.begin(), package_cc.begin(), package_cc.end());
	std::vector<std::filesystem::path> obj_files;
	for (auto const& cc : package_cc) {
		obj_files.push_back(cc.obj_file);
	}
	auto lc = make_link_command(obj_files, package, package_graph.all_deps(package),
				    package_build_folder);
	link_commands.push_back(lc);
}

CompileCommand make_compile_command(std::filesystem::path const& source_file,
				    Package const& package,
				    std::vector<Package> const& dependencies,
				    std::filesystem::path const& output_folder)
{
	std::filesystem::path obj_file = output_folder / (source_file.filename().string() + ".o");
	std::stringstream cmd;
	cmd << "clang++" // TODO: CompilationSettings
	    << " -Wall"	 // TODO: CompilationOptions
	    << " -c " << source_file.generic_string()
	    << " -std=" << package.std // TODO: ABI compatibility warnings
	    << " -o " << obj_file.generic_string();
	for (auto const& incl : package.includes) {
		cmd << " -I\"" << incl.generic_string() << "\"";
	}
	for (auto const& dep : dependencies) {
		for (auto const& public_incl : dep.public_includes) {
			spdlog::trace("Adding public include {} of the dependency {} of package {}",
				      public_incl.generic_string(), dep.name, package.name);
			cmd << " -I\"" << public_incl << "\"";
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
			      std::filesystem::path const& output_folder)
{
	auto output_file_path = output_folder / package.id();
	std::stringstream cmd;
	switch (package.type) {
	case Application: {
		cmd << "clang++"; // TODO: CompilationSettings
		for (auto const& o : obj_files) {
			cmd << " " << o;
		}
		for (auto const& dep : dependencies) {
			auto path = output_folder / dep.id() / (dep.name + ".a");
			cmd << " \"" << path.generic_string() << "\"";
		}
		cmd << " -o " << output_file_path.generic_string();
		break;
	}
	case StaticLibrary: {
		cmd << "ar r " << output_file_path.generic_string() + ".a";
		for (auto const& o : obj_files) {
			cmd << " " << o.generic_string();
		}
		break;
	}
	}
	LinkCommand command;
	command.cmd = cmd.str();
	command.obj_files = obj_files;
	command.binary = output_file_path;
	return command;
}

} // namespace autob