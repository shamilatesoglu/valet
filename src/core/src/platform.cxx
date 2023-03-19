#include "valet/platform.hxx"
#include "valet/string_utils.hxx"

namespace valet::platform
{

std::string static_link_command_prefix(std::string const& output_file_path_str)
{
#if defined(_WIN32)
	return "lld-link -lib /out:\"" + output_file_path_str + "\"";
#elif defined(__APPLE__)
	return "ld -r -o " + output_file_path_str;
#elif defined(__linux__)
	return "ar r " + output_file_path_str;
#else
#error Unsupported platform
#endif
}

void sanitize_path(std::string& path)
{
#if defined(_WIN32)
	// Replace "\ " with "".
	util::replace_string(path, "\\ ", " ");
	util::replace_string(path, "/ ", " ");
	util::replace_string(path, "\"", "");
#endif
}

bool clangpp_test()
{
	return true;
}

void escape_cli_command(std::string& command)
{
#if defined(_WIN32)
	util::replace_string(command, "\"", "\\\"");
	util::replace_string(command, "=", "^=");
#endif
}

std::filesystem::path get_home_dir()
{
#if defined(_WIN32)
#if defined(_MSC_VER)
	// Use _dupenv_s.
	char* home_dir = nullptr;
	size_t home_dir_size = 0;
	_dupenv_s(&home_dir, &home_dir_size, "USERPROFILE");
	std::filesystem::path home_dir_path(home_dir);
	free(home_dir);
	return home_dir_path;
#else
	return std::filesystem::path(std::getenv("USERPROFILE"));
#endif
#elif defined(__APPLE__)
	return std::filesystem::path(std::getenv("HOME"));
#elif defined(__linux__)
	return std::filesystem::path(std::getenv("HOME"));
#else
#error Unsupported platform
#endif
}

} // namespace valet::platform