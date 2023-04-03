#pragma once

// stl
#include <string>
#include <filesystem>

namespace valet::platform
{

std::string static_link_command_prefix(std::string const& output_file_path_str);
void sanitize_path(std::string& path);
bool clangpp_test();
void escape_cli_command(std::string& command);
std::filesystem::path get_home_dir();
uint32_t get_cpu_count();

} // namespace valet::platform