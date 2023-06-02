#pragma once

#include <string>
#include <vector>
#include <sstream>

inline std::vector<std::string> str_split(const std::string& in, const char seperator)
{
  std::vector<std::string> output;
  std::istringstream stream(in);
  for (std::string s; std::getline(stream, s, seperator); ) {
    output.push_back(s);
  }
  return output;
}
