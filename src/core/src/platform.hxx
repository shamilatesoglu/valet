#pragma once

// stl
#include <string>
#include <filesystem>

namespace valet::platform
{

#if _WIN32
constexpr const char* SHARED_LIB_EXT = ".dll";
constexpr const char* STATIC_LIB_EXT = ".lib";
constexpr const char* EXECUTABLE_EXT = ".exe";
#elif __APPLE__
constexpr const char* SHARED_LIB_EXT = ".dylib";
constexpr const char* STATIC_LIB_EXT = ".a";
constexpr const char* EXECUTABLE_EXT = "";
#elif __linux__
constexpr const char* SHARED_LIB_EXT = ".so";
constexpr const char* STATIC_LIB_EXT = ".a";
constexpr const char* EXECUTABLE_EXT = "";
#endif

std::string static_link_command_prefix(std::string const& output_file_path_str);
void sanitize_path(std::string& path);
void escape_cli_command(std::string& command);
std::filesystem::path home_dir();
std::filesystem::path valet_dir();
// Directory where valet stores the remote dependencies.
std::filesystem::path garage_dir();
std::filesystem::path temp_dir();
uint32_t get_cpu_count();

} // namespace valet::platform