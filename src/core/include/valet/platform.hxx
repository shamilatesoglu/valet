#pragma once

// std
#include <string>
#include <filesystem>

namespace valet::platform
{

std::string static_link_command_prefix(std::string const& output_file_path_str);
void add_executable_file_ext(std::string& output_file_path_str);
void sanitize_path(std::string& path);
bool clangpp_test();
void escape_cli_command(std::string& command);

} // namespace valet::platform