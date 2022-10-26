#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "../third_party/spdlog/spdlog.h"
#include "../third_party/tomlplusplus/toml.hpp"

struct package_t {
  std::string name;
  std::string version;
  std::string std;
  std::filesystem::path compiler_path;
  std::vector<std::filesystem::path> include_paths;
};

std::optional<std::filesystem::path>
find_autob(std::filesystem::path const &folder) {
  for (auto const &entry : std::filesystem::directory_iterator(folder)) {
    if (entry.path().extension() == ".toml") {
      return entry.path();
    }
  }
  return std::nullopt;
}

void collect_source_files(std::filesystem::path const &folder,
                          std::vector<std::filesystem::path> &out) {
  static const std::unordered_set<std::string> source_file_extensions = {
      ".cxx", ".c", ".cpp", ".c++"};
  for (auto const &entry : std::filesystem::directory_iterator(folder)) {
    if (entry.is_directory()) {
      if (auto nested_autob = find_autob(entry.path())) {
        spdlog::info("Found another package: {}",
                     nested_autob->generic_string());
        // TODO: Figure out how to do nested packages.
        continue;
      }
      collect_source_files(entry.path(), out);
    } else if (source_file_extensions.contains(entry.path().extension())) {
      out.push_back(entry.path());
      spdlog::info("Found source file: {}", entry.path().generic_string());
    }
  }
}

std::optional<std::filesystem::path>
compile(package_t const &package, std::filesystem::path const &output_folder,
        std::filesystem::path const &source_file) {
  std::filesystem::path obj_file =
      output_folder / (source_file.filename().string() + ".o");
  std::stringstream cmd;
  cmd << package.compiler_path.generic_string() << " -c "
      << source_file.generic_string() << " -std=" << package.std << " -o "
      << obj_file.generic_string();
  for (auto const &incl : package.include_paths) {
    cmd << " -I" << incl.generic_string();
  }
  auto cmdstr = cmd.str();
  spdlog::debug("Compile command: {}", cmdstr);
  spdlog::info("Compiling: {}", source_file.generic_string());
  int ret = std::system(cmdstr.c_str());
  if (ret != 0) {
    spdlog::error("Failed to compile {}. Return code: {}",
                  source_file.generic_string(), ret);
    return std::nullopt;
  }
  return obj_file;
}

bool link(package_t const &package,
          std::vector<std::filesystem::path> const &obj_files,
          std::filesystem::path const &output_folder) {
  auto output_file_path = output_folder / package.name;
  std::stringstream cmd;
  cmd << package.compiler_path.generic_string();
  for (auto const &obj : obj_files) {
    cmd << " " << obj;
  }
  cmd << " -o " << output_file_path;
  auto cmdstr = cmd.str();
  spdlog::debug("Link command: {}", cmdstr);
  spdlog::info("Linking: {}", output_file_path.generic_string());
  int ret = std::system(cmdstr.c_str());
  if (ret != 0) {
    spdlog::error("Failed to link. Return code: {}", ret);
    return false;
  }
  return true;
}

package_t parse_autob(std::filesystem::path const &cfg_file_path) {
  auto cfg_tbl = toml::parse_file(cfg_file_path.generic_string());
  auto const &package_toml = cfg_tbl["package"];
  package_t package{
      package_toml["name"].value_or<std::string>(""),
      package_toml["version"].value_or<std::string>(""),
      package_toml["std"].value_or<std::string>(""),
      package_toml["compiler_path"].value_or<std::string>(""),
  };
  auto const &include_paths_ref = package_toml["include_paths"];
  if (auto path_arr = include_paths_ref.as_array()) {
    for (auto &&path_node : *path_arr) {
      package.include_paths.push_back(*path_node.value<std::string>());
    }
  }
  return package;
}

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("%^[%=8l] %v%$");
  try {
    std::filesystem::path work_folder(argv[1]);
    std::filesystem::path output_folder = work_folder / "build";
    std::filesystem::path source_folder = work_folder / "src";
    std::filesystem::path test_folder = work_folder / "test";

    auto cfg_file_path = find_autob(work_folder);
    if (!cfg_file_path) {
      spdlog::error("Cannot find autob config file under directory {}",
                    work_folder.generic_string());
      return 1;
    }
    spdlog::info("Using autob configuration {}",
                 cfg_file_path->generic_string());

    package_t package = parse_autob(*cfg_file_path);

    std::filesystem::create_directory(output_folder);

    std::vector<std::filesystem::path> source_files;
    collect_source_files(source_folder, source_files);
    std::vector<std::filesystem::path> obj_files;
    for (auto const &src : source_files) {
      if (auto obj_file = compile(package, output_folder, src)) {
        obj_files.push_back(*obj_file);
      }
    }

    if (link(package, obj_files, output_folder)) {
      spdlog::info("Success.");
      return 0;
    } else
      return -1;

  } catch (const toml::parse_error &err) {
    spdlog::error("Parsing failed: {}", err.description());
    return 1;
  }

  return 0;
}
