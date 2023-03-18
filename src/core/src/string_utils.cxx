#include "valet/string_utils.hxx"

namespace valet::util
{

std::vector<std::string> split(std::string const& str, std::string const& delim)
{
	std::vector<std::string> result;
	size_t start = 0;
	size_t end = 0;
	while ((end = str.find(delim, start)) != std::string::npos) {
		result.push_back(str.substr(start, end - start));
		start = end + delim.length();
	}
	result.push_back(str.substr(start));
	return result;
}

void replace_string(std::string& subject, const std::string& search, const std::string& replace)
{
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

void trim(std::string& str)
{
	str.erase(str.find_last_not_of(' ') + 1); // suffixing spaces
	str.erase(0, str.find_first_not_of(' ')); // prefixing spaces
}

void strip(std::string& str)
{
	if (str.length() == 0) {
		return;
	}

	auto start_it = str.begin();
	auto end_it = str.rbegin();
	while (std::isspace(*start_it)) {
		++start_it;
		if (start_it == str.end())
			break;
	}
	while (std::isspace(*end_it)) {
		++end_it;
		if (end_it == str.rend())
			break;
	}
	int start_pos = start_it - str.begin();
	int end_pos = end_it.base() - str.begin();
	str = start_pos <= end_pos ? std::string(start_it, end_it.base()) : "";
}

} // namespace valet::util