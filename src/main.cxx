#define VALET_VERSION "0.1"

// valet
#include <valet/build.hxx>
#include <valet/install.hxx>
#include <valet/stopwatch.hxx>

// external
#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>

// std
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// TODO: Lots of copies are being made during package graph creation and building. Optimize.

std::optional<std::filesystem::path> path_from_str(std::string const& path_str, bool should_exist,
						   bool is_dir)
{
	std::filesystem::path path(path_str);
	if (should_exist && !std::filesystem::exists(path)) {
		spdlog::error("No such file or directory: {}", path.generic_string());
		return std::nullopt;
	}
	if (is_dir && !std::filesystem::is_directory(path)) {
		spdlog::error("Not a directory: {}", path.generic_string());
		return std::nullopt;
	} else if (!is_dir && std::filesystem::is_directory(path)) {
		spdlog::error("Expected a file, is directory: {}", path.generic_string());
		return std::nullopt;
	}
	try {
		path = std::filesystem::canonical(path);
	} catch (std::filesystem::filesystem_error const& err) {
		spdlog::error("{}", err.what());
		return std::nullopt;
	}
	return path;
}

int main(int argc, char* argv[])
{
	argparse::ArgumentParser program("valet", VALET_VERSION);

	// Common arguments
	program.add_argument("--verbose", "-v")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Print helpful debug information");
	program.add_argument("--dry-run", "-dr")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Do not execute build. Useful with --verbose.");

	argparse::ArgumentParser build("build", VALET_VERSION);
	build.add_description(
	    "Builds a package in the current source tree. If no target is specified, "
	    "builds the target found in current working directory.");
	build.add_argument("--source", "-s")
	    .default_value(std::string("./"))
	    .help("Root folder of the package to build");
	build.add_argument("--release", "-r")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Build in release mode with optimizations");
	build.add_argument("--clean").default_value(false).implicit_value(true).help(
	    "Clean build folder");
	build.add_argument("--export-compile-commands", "-ecc")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Export compilation commands database");
	program.add_subparser(build);

	argparse::ArgumentParser run("run", VALET_VERSION);
	run.add_description("Runs a binary. If needed, builds the target first.");
	run.add_argument("--target", "-t")
	    .nargs(argparse::nargs_pattern::at_least_one)
	    .help("Runs the specified target in the source tree. If no target is "
		  "specified, runs the target found in current working directory (due to default "
		  "value of --source).");
	run.add_argument("--source", "-s")
	    .default_value(std::string("./"))
	    .help("Root folder of the package to run");
	run.add_argument("--release", "-r")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Build in release mode with optimizations");
	run.add_argument("--clean").default_value(false).implicit_value(true).help(
	    "Clean build folder");
	program.add_subparser(run);

	argparse::ArgumentParser update("update", VALET_VERSION);
	update.add_description("Updates the dependencies of the package.");
	update.add_argument("--offline")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Tries to update package dependencies without accessing the network");
	program.add_subparser(update);

	argparse::ArgumentParser install("install", VALET_VERSION);
	install.add_description(
	    "Installs a package from a remote repository or from a local path.");
	install.add_argument("--source", "-s").help("Path to the package to install");
	install.add_argument("package")
	    .nargs(argparse::nargs_pattern::any)
	    .help("Package to install");
	program.add_subparser(install);

	try {
		program.parse_args(argc, argv);
	} catch (const std::runtime_error& err) {
		std::cout << err.what() << std::endl;
		std::cout << program;
		return 0;
	}

	spdlog::set_level(program.get<bool>("verbose")
			      ? spdlog::level::debug
			      : spdlog::level::info); // TODO: Make this configurable
	spdlog::set_pattern("%^[%=8l] %v%$");

	if (program.is_subcommand_used("install")) {
		auto const& packages = install.get<std::vector<std::string>>("package");
		if (!packages.empty() && install.is_used("source")) {
			spdlog::error("Install: Cannot specify both package name (remote "
				      "installation) and source path (local installation)");
			return 1;
		}
		if (!packages.empty()) {
			spdlog::error("Valet does not yet support remote package installation");
			return 1;
		}
		if (install.is_used("source")) {
			auto source_path_opt =
			    path_from_str(install.get<std::string>("source"), true, true);
			if (!source_path_opt) {
				return 1;
			}
			if (!valet::install_local_package(source_path_opt.value(),
							  valet::get_default_install_path())) {
				spdlog::error("Install failed");
				return 1;
			}
			spdlog::info("Install succeeded");
		}
		return 0;
	}
	if (program.is_subcommand_used("build")) {
		valet::util::Stopwatch stopwatch;
		auto project_folder_opt = path_from_str(build.get("source"), true, true);
		if (!project_folder_opt) {
			return 1;
		}
		auto project_folder = *project_folder_opt;
		if (!valet::build(
			{.project_folder = project_folder,
			 .compile_options =
			     valet::CompileOptions{
				 .release = build.get<bool>("release"),
			     },
			 .dry_run = program.get<bool>("dry-run"),
			 .clean = build.get<bool>("clean"),
			 .export_compile_commands = build.get<bool>("export-compile-commands"),
			 .collect_stats = false})) {
			spdlog::error("Build failed");
			return 1;
		}
		spdlog::info("Build succeeded ({})", stopwatch.elapsed_str());
		return 0;
	}
	if (program.is_subcommand_used("run")) {
		bool release = run.get<bool>("release");
		bool clean = run.get<bool>("clean");

		auto project_folder_opt = path_from_str(run.get("source"), true, true);
		if (!project_folder_opt) {
			return 1;
		}
		auto project_folder = *project_folder_opt;
		valet::RunParams params;
		params.build.compile_options.release = release;
		params.build.clean = clean;
		params.build.project_folder = *project_folder_opt;
		params.targets = run.get<std::vector<std::string>>("target");

		if (!valet::run(params)) {
			spdlog::error("Run failed");
			return 1;
		}
		return 0;
	}
	if (program.is_subcommand_used("update")) {
		bool offline = update.get<bool>("offline");
		spdlog::warn("Update command not implemented yet.");
		return 1;
	}
	spdlog::error("No subcommand specified");
	std::cout << program;
	return 1;
}
