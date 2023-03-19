#include <valet/install.hxx>
#include <valet/build.hxx>
#include <valet/platform.hxx>

#include <filesystem>

namespace valet
{

std::filesystem::path get_default_install_path()
{
	return platform::get_home_dir() / ".valet" / "bin";
}

bool install_local_package(std::filesystem::path const& project_folder,
			   std::filesystem::path const& install_path)
{
	auto package = find_package(project_folder);
	if (!package)
		return false;

	BuildParams params;
	params.project_folder = package->folder;
	params.compile_options.release = true;
	params.clean = true;
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