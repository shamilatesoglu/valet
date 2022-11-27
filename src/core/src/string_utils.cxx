#include "autob/string_utils.hxx"

namespace autob::util
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

} // namespace autob::util