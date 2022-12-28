#pragma once

// std
#include <string>
#include <filesystem>

namespace autob::platform
{

std::string static_link_command_prefix(std::string const& output_file_path_str);
void add_executable_file_ext(std::string& output_file_path_str);
void sanitize_path(std::string& path);
bool clangpp_test();

} // namespace autob::platform