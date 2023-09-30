#include "valet/command.hxx"

// valet
#include "valet/stopwatch.hxx"
#include "string_utils.hxx"
#include "platform.hxx"

// External
#include <spdlog/spdlog.h>

// stl
#include <cstdlib>
#include <sstream>

namespace valet
{

CompileCommand::CompileCommand(Package const& package, std::filesystem::path const& source_file,
			       std::vector<Package> const& dependencies, CompileOptions const& opts,
			       std::filesystem::path const& output_folder)
    : package(package), source_file(source_file),
      obj_file(output_folder / (source_file.filename().string() + ".o")),
      dependencies(dependencies), opts(opts)
{
}

std::string CompileCommand::string() const
{
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
	return cmd.str();
}

LinkCommand::LinkCommand(Package const& package,
			 std::vector<std::filesystem::path> const& obj_files,
			 std::vector<Package> const& dependencies,
			 std::filesystem::path const& output_folder)
    : package(package), obj_files(obj_files), dependencies(dependencies),
      binary_path(output_folder / package.id / package.name)
{
}

std::string LinkCommand::string() const
{
	auto output_file_path_str = binary_path.generic_string();
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
			auto dep_bin_path_no_ext =
			    binary_path.parent_path().parent_path() / dep.id / dep.name;
			auto dep_bin_path_str = dep_bin_path_no_ext.generic_string();
			if (dep.type == Package::Type::SharedLibrary) {
				spdlog::error("Sorry, but valet does not support linking against a "
					      "shared library yet. Offending package: {}",
					      dep.id);
				exit(1);
			}
			dep_bin_path_str += dep.target_ext();
			// TODO: If header only?
			cmd << " " << dep_bin_path_str;
		}
		cmd << " -o " << output_file_path_str + package.target_ext();
		if (package.type == Package::Type::SharedLibrary) {
#if defined(_WIN32)
#elif defined(__APPLE__)
			cmd << " -Wl,-undefined,dynamic_lookup";
#elif defined(__linux__)
			auto soname = binary_path.filename().generic_string();
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
	return cmd.str();
}

int execute(std::string const& command, std::optional<std::filesystem::path> const& working_dir)
{
	util::Stopwatch stopwatch;
	std::string cmd;
	if (working_dir) {
		cmd = "cd " + working_dir->generic_string() + " && " + command;
	} else {
		cmd = command;
	}
	spdlog::trace("Executing: {}", cmd);
	auto ret = std::system(cmd.c_str());
	if (ret) {
		spdlog::error("Command failed with return code {}", ret);
	}
	spdlog::trace("Command took {}", stopwatch.elapsed_str());
	return ret;
}

} // namespace valet