#include "valet/install.hxx"

// valet
#include "valet/build.hxx"
#include "platform.hxx"

// stl
#include <filesystem>

namespace valet
{

std::filesystem::path get_default_install_path()
{
	return platform::valet_dir() / "bin";
}

bool install_local_package(std::filesystem::path const& project_folder,
			   std::filesystem::path const& install_path)
{
	auto package = find_package(project_folder);
	if (!package) {
		spdlog::error("Cannot find valet config file under directory {}",
			      project_folder.generic_string());
		return false;
	}
	BuildParams params;
	params.project_folder = package->folder;
	params.compile_options.release = true;
	params.clean = true; // Remove this when we are confident that incremental builds are
			     // working absolutely correct
	params.dry_run = false;
	params.export_compile_commands = false;
	params.collect_stats = false;

	BuildPlan plan;
	if (!build(params, &plan)) {
		return false;
	}
	auto target_path = package->target_path(true);
	spdlog::info("Installing {} to {}", target_path.generic_string(),
		     install_path.generic_string());
	try {
		std::filesystem::create_directories(install_path);
		std::filesystem::copy(target_path, install_path / target_path.filename(),
				      std::filesystem::copy_options::overwrite_existing);
	} catch (std::exception const& e) {
		spdlog::error("Failed to copy {} to {}: {}", target_path.generic_string(),
			      install_path.generic_string(), e.what());
		return false;
	}
	return true;
}

} // namespace valet