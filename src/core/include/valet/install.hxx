#pragma once

// stl
#include <filesystem>

namespace valet
{

std::filesystem::path get_default_install_path();

bool install_local_package(std::filesystem::path const& project_folder,
			   std::filesystem::path const& install_path);

} // namespace valet