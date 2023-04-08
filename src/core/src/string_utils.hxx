#pragma once

// stl
#include <vector>
#include <string>

namespace valet::util
{

std::vector<std::string> split(std::string const& str, std::string const& delim);

void replace_string(std::string& subject, const std::string& search, const std::string& replace);

void trim(std::string& str);

void strip(std::string& str);

std::string to_lower(std::string str);

std::string to_upper(std::string str);

} // namespace valet::util