#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "../third_party/toml.hpp"

struct package_t {
  std::string name;
  std::string version;
  std::string std;
  std::filesystem::path compiler_path;
};

std::vector<std::filesystem::path>
collect_source_files(std::filesystem::path const &folder) {
  static const std::unordered_set<std::string> source_file_extensions = {
      ".cxx", ".c", ".cpp", ".c++"};
  std::vector<std::filesystem::path> out;
  for (auto const &entry :
       std::filesystem::recursive_directory_iterator(folder)) {
    if (source_file_extensions.contains(entry.path().extension())) {
      out.push_back(entry.path());
      std::cout << "Found source file: " << entry.path().generic_string()
                << std::endl;
    }
  }
  return out;
}

std::optional<std::filesystem::path>
compile(package_t const &package, std::filesystem::path const &output_folder,
        std::filesystem::path const &source_file) {
  std::filesystem::path obj_file =
      output_folder / (source_file.filename().string() + ".o");
  std::string cmd = package.compiler_path.generic_string() + " -c " +
                    source_file.generic_string() + " -std=" + package.std +
                    " -o " + obj_file.generic_string();
  int ret = std::system(cmd.c_str());
  if (ret != 0) {
    std::cout << "Failed to compile " << source_file << ". Return code: " << ret
              << std::endl;
    return std::nullopt;
  }
  return obj_file;
}

bool link(package_t const &package,
          std::vector<std::filesystem::path> const &obj_files,
          std::filesystem::path const &output_folder) {
  std::stringstream cmd;
  cmd << package.compiler_path.generic_string();
  for (auto const &obj : obj_files) {
    cmd << " " << obj;
  }
  cmd << " -o " << (output_folder / package.name);

  int ret = std::system(cmd.str().c_str());
  if (ret != 0) {
    std::cout << "Failed to link. Return code: " << ret << std::endl;
    return false;
  }
  return false;
}

std::filesystem::path find_autob_config(std::filesystem::path const &folder) {
  for (auto const &entry : std::filesystem::directory_iterator(folder)) {
    if (entry.path().extension() == ".toml") {
      std::cout << "Using autob configuration " << entry.path().generic_string()
                << std::endl;
      return entry.path();
    }
  }
  return std::filesystem::path();
}

int main(int argc, char *argv[]) {
  std::cout << "[autob]" << std::endl;

  try {
    std::filesystem::path work_folder(argv[1]);
    std::filesystem::path output_folder = work_folder / "build";

    auto cfg = find_autob_config(work_folder);
    if (cfg.empty()) {
      std::cout << "Cannot find autob config file under directory "
                << work_folder << std::endl;
      return 1;
    }

    auto cfg_tbl = toml::parse_file(cfg.generic_string());
    auto const &package_toml = cfg_tbl["package"];
    package_t package{
        package_toml["name"].value_or<std::string>(""),
        package_toml["version"].value_or<std::string>(""),
        package_toml["std"].value_or<std::string>(""),
        package_toml["compiler_path"].value_or<std::string>(""),
    };

    std::filesystem::create_directory(output_folder);

    auto source_files = collect_source_files(work_folder);
    std::vector<std::filesystem::path> obj_files;
    for (auto const &src : source_files) {
      std::cout << "Compiling: " << src.generic_string() << std::endl;
      if (auto obj_file = compile(package, output_folder, src)) {
        obj_files.push_back(*obj_file);
      }
    }

    if (link(package, obj_files, output_folder)) {
      std::cout << "Success." << std::endl;
      return 0;
    } else
      return -1;

  } catch (const toml::parse_error &err) {
    std::cerr << "Parsing failed:" << std::endl << err << std::endl;
    return 1;
  }

  return 0;
}
