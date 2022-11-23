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

	program.add_argument("--source", "-s").default_value(".").help("Source folder to build");
	program.add_argument("--verbose", "-v")
	    .default_value(false)
	    .implicit_value(true)
	    .help("Print helpful debug information");

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
		std::filesystem::path project_folder = program.get<std::string>("source");
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
			if (!build_plan->execute_plan()) {
				spdlog::error("Build failed.");
				return 1;
			}
			spdlog::info("Success.");
		}
	} catch (const toml::parse_error& err) {
		spdlog::error("Parsing failed: {}", err.description());
		return 1;
	}
	return 0;
}
