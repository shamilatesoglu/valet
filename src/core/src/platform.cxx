#include "autob/platform.hxx"
#include "autob/string_utils.hxx"

namespace autob::platform
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

void add_executable_file_ext(std::string& output_file_path_str)
{
#if defined(_WIN32)
	output_file_path_str += ".exe";
#endif
}

void sanitize_path(std::string &path)
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

} // namespace autob::platform