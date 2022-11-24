// std
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// external
#include <spdlog/spdlog.h>
#include <tomlplusplus/toml.hpp>
#include <argparse/argparse.hpp>

// autob
#include "build.hxx"

int main(int argc, char* argv[])
{
	argparse::ArgumentParser program("autob", "0.1");

	program.add_argument("--source", "-s")
	    .default_value(std::string("./"))
	    .help("Source folder to build");
	program.add_argument("--run", "-r").help("Runs the specified target");
	program.add_argument("--verbose", "-v")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Print helpful debug information");
	program.add_argument("--export-compile-commands", "-ecc")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Export compilation commands database");

	try {
		program.parse_args(argc, argv);
	} catch (std::runtime_error const& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	spdlog::set_level(program.get<bool>("verbose") ? spdlog::level::trace
						       : spdlog::level::info);
	spdlog::set_pattern("%^[%=8l] %v%$");

	try {
		std::string path_string = program.get("source");
		std::filesystem::path project_folder = path_string;
		project_folder = std::filesystem::canonical(project_folder);
		std::filesystem::path build_folder = project_folder / "build";
		if (auto package_graph = autob::make_package_graph(project_folder)) {
			if (package_graph->empty()) {
				spdlog::error("No package found under {}",
					      project_folder.generic_string());
				return 1;
			}
			auto build_plan = autob::make_build_plan(*package_graph, build_folder);
			if (!build_plan) {
				spdlog::error("Unable to generate build plan");
				return 1;
			}
			if (program.get<bool>("export-compile-commands")) {
				build_plan->export_compile_commands(project_folder);
			}
			if (!build_plan->execute_plan()) {
				spdlog::error("Build failed.");
				return 1;
			}
			spdlog::info("Build success.");
			if (program.is_used("run")) {
				std::string target_name = program.get<std::string>("run");
				auto executable =
				    build_plan->get_executable_target_by_name(target_name);
				if (!executable) {
					spdlog::error("No such executable target: {}", target_name);
					return 1;
				}
				std::filesystem::path exe_path =
				    build_folder / executable->id / executable->name;
				return std::system(exe_path.generic_string().c_str());
			}
		}
	} catch (const toml::parse_error& err) {
		spdlog::error("Parsing failed: {}", err.description());
		return 1;
	}
	return 0;
}
