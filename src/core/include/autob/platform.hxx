#pragma once

// std
#include <string>
#include <filesystem>

namespace autob::platform
{

std::string static_link_command_prefix(std::string const& output_file_path_str);
bool clangpp_test();

} // namespace autob::platform