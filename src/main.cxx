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

// autob
#include "build.hxx"

int main(int argc, char* argv[])
{
	spdlog::set_level(spdlog::level::trace);
	spdlog::set_pattern("%^[%=8l] %v%$");
	try {
		std::filesystem::path project_folder = std::filesystem::current_path();
		std::filesystem::path build_folder = project_folder / "build";
		if (auto package_graph = autob::make_package_graph(project_folder)) {
			auto build_plan = autob::make_build_plan(*package_graph, build_folder);
                        // TODO
		}
	} catch (const toml::parse_error& err) {
		spdlog::error("Parsing failed: {}", err.description());
		return 1;
	}
	return 0;
}
