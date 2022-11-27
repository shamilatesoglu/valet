#pragma once

#include <vector>
#include <string>

namespace autob::util
{

std::vector<std::string> split(std::string const& str, std::string const& delim);

void replace_string(std::string& subject, const std::string& search, const std::string& replace);

void trim(std::string& str);

} // namespace autob::util