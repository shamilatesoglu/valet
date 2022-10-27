#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include <tomlplusplus/toml.hpp>

namespace stdfs = std::filesystem;

enum package_type {
  application,
  static_library,
};

struct dependency_info_t {
  std::string name;
  stdfs::path path;
  std::optional<std::string> version = std::nullopt;
};

struct package_t {
  std::string name;
  std::string version;
  std::string std;
  package_type type;
  stdfs::path compiler_path;
  std::vector<stdfs::path> public_includes;
  std::vector<stdfs::path> includes;
  std::vector<dependency_info_t> dependencies; // TODO: Figure this out
  std::vector<package_t> resolved_dependencies;
};

std::optional<stdfs::path> find_autob(stdfs::path const &folder) {
  for (auto const &entry : stdfs::directory_iterator(folder)) {
    if (entry.path().extension() == ".toml") {
      return entry.path();
    }
  }
  return std::nullopt;
}

void collect_source_files(stdfs::path const &folder,
                          std::vector<stdfs::path> &out) {
  static const std::unordered_set<std::string> source_file_extensions = {
      ".cxx", ".cpp", ".c++", ".c"};
  for (auto const &entry : stdfs::directory_iterator(folder)) {
    if (entry.is_directory()) {
      if (auto nested_autob = find_autob(entry.path())) {
        // TODO: Figure out how to do nested packages.
        continue;
      }
      collect_source_files(entry.path(), out);
    } else if (source_file_extensions.contains(
                   entry.path().extension().string())) {
      auto source_file = stdfs::canonical(entry.path());
      out.push_back(source_file);
      spdlog::debug("Found source file: {}", source_file.generic_string());
    }
  }
}

std::optional<stdfs::path> compile(package_t const &package,
                                   stdfs::path const &output_folder,
                                   stdfs::path const &source_file) {
  stdfs::path obj_file =
      output_folder / (source_file.filename().string() + ".o");
  std::stringstream cmd;
  cmd << package.compiler_path.generic_string() << " -Wall"
      << " -c " << source_file.generic_string() << " -std=" << package.std
      << " -o " << obj_file.generic_string();
  for (auto const &incl : package.includes) {
    cmd << " -I\"" << incl.generic_string() << "\"";
  }
  for (auto const &dep : package.resolved_dependencies) {
    for (auto const &public_include : dep.public_includes) {
      spdlog::trace(
          "Adding public include {} of the dependency {} of package {}",
          public_include.generic_string(), dep.name, package.name);
      cmd << " -I" << public_include;
    }
  }
  auto cmdstr = cmd.str();
  spdlog::info("Compiling: {}", source_file.generic_string());
  spdlog::debug("Compile command: {}", cmdstr);
  int ret = std::system(cmdstr.c_str());
  if (ret != 0) {
    spdlog::error("Failed to compile {}. Return code: {}",
                  source_file.generic_string(), ret);
    return std::nullopt;
  }
  return obj_file;
}

void add_lib_paths(std::stringstream &cmd, package_t const& package, stdfs::path const& output_folder) {
  for (auto const &dep : package.resolved_dependencies) {
    cmd << " \"" << (output_folder / dep.name / dep.name).generic_string()
        << ".a\"";
    add_lib_paths(cmd, dep, output_folder / dep.name);
  }
}

bool link(package_t const &package, std::vector<stdfs::path> const &obj_files,
          stdfs::path const &output_folder) {
  auto output_file_path = output_folder / package.name;
  std::stringstream cmd;
  switch (package.type) {
  case application:
    cmd << package.compiler_path.generic_string();
    for (auto const &obj : obj_files) {
      cmd << " " << obj;
    }
    add_lib_paths(cmd, package, output_folder); // TODO: Should not link a lib twice
    cmd << " -o " << output_file_path;
    break;
  case static_library:
    cmd << "ar r " << output_file_path.generic_string() + ".a";
    for (auto const &obj : obj_files) {
      cmd << " " << obj;
    }
    break;
  }
  auto cmdstr = cmd.str();
  spdlog::info("Linking: {}", output_file_path.generic_string());
  spdlog::debug("Link command: {}", cmdstr);
  int ret = std::system(cmdstr.c_str());
  if (ret != 0) {
    spdlog::error("Failed to link. Return code: {}", ret);
    return false;
  }
  return true;
}

package_t parse_autob(stdfs::path const &cfg_file_path) {
  auto package_folder = cfg_file_path.parent_path();
  auto cfg_tbl = toml::parse_file(cfg_file_path.generic_string());
  auto const &package_toml = cfg_tbl["package"];
  package_t package;
  package.name = package_toml["name"].value_or<std::string>("");
  package.version = package_toml["version"].value_or<std::string>("");
  package.std = package_toml["std"].value_or<std::string>("");
  package.compiler_path =
      package_toml["compiler_path"].value_or<std::string>("");
  auto package_type_str = package_toml["type"].value_or<std::string>("");
  if (package_type_str == "bin") {
    package.type = package_type::application;
  } else if (package_type_str == "lib") {
    package.type = package_type::static_library;
  }
  auto const &includes_ref = package_toml["includes"];
  if (auto path_arr = includes_ref.as_array()) {
    for (auto &&path_node : *path_arr) {
      stdfs::path path = *path_node.value<std::string>();
      path = stdfs::canonical(package_folder / path);
      package.includes.push_back(path);
    }
  }
  auto const &public_includes_ref = package_toml["public_includes"];
  if (auto path_arr = public_includes_ref.as_array()) {
    for (auto &&path_node : *path_arr) {
      stdfs::path path = *path_node.value<std::string>();
      path = stdfs::canonical(package_folder / path);
      package.public_includes.push_back(path);
    }
  }
  if (!cfg_tbl.contains("dependencies")) {
    return package;
  }
  auto const &dependencies_toml = cfg_tbl["dependencies"];
  for (auto const &entry : *dependencies_toml.node()->as_table()) {
    dependency_info_t dep_nfo;
    dep_nfo.name = entry.first.str();
    dep_nfo.path = stdfs::canonical(package_folder / entry.second.as_string()->get());
    package.dependencies.push_back(dep_nfo);
  }
  return package;
}

std::optional<package_t> build(stdfs::path const &package_folder,
                               stdfs::path const &output_folder) {
  spdlog::trace("Entering package folder: {}", package_folder.generic_string());
  auto cfg_file_path = find_autob(package_folder);
  if (!cfg_file_path) {
    spdlog::error("Cannot find autob config file under directory {}",
                  package_folder.generic_string());
    return std::nullopt;
  }
  spdlog::info("Using autob configuration {}", cfg_file_path->generic_string());

  package_t package = parse_autob(*cfg_file_path);

  for (auto const &dep : package.dependencies) {
    if (auto resolved = build(dep.path, output_folder / dep.name)) {
      package.resolved_dependencies.push_back(*resolved);
    }
  }

  stdfs::path source_folder = package_folder / "src";
  stdfs::path test_folder = package_folder / "test";

  stdfs::create_directories(output_folder);

  std::vector<stdfs::path> source_files;
  collect_source_files(source_folder, source_files);
  std::vector<stdfs::path> obj_files;
  for (auto const &src : source_files) {
    if (auto obj_file = compile(package, output_folder, src)) {
      obj_files.push_back(*obj_file);
    }
  }

  if (!link(package, obj_files, output_folder)) {
    return std::nullopt;
  }
  return package;
}

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::trace);
  spdlog::set_pattern("%^[%=8l] %v%$");
  try {
    stdfs::path package_folder(argv[1]);
    stdfs::path output_folder = package_folder / "build";
    if (build(package_folder, output_folder)) { // TODO: Spew out compile_commands.json
      spdlog::info("Success.");
    }
  } catch (const toml::parse_error &err) {
    spdlog::error("Parsing failed: {}", err.description());
    return 1;
  }
  return 0;
}
