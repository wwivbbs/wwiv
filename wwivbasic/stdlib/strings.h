#pragma once

#include "wwivbasic/value.h"
#include <string>
#include <vector>

namespace wwivbasic::stdlib {

int asc(std::string s);
std::string chr(int c);

std::string left(std::string s, int len);
std::string right(std::string s, int len);
Value mid(std::vector<Value> args);
std::string ucase(std::string s);
std::string lcase(std::string s);
std::string space(int n);
std::string string_of(int n, Value c);
std::string trim(std::string s);
std::string ltrim(std::string s);
std::string rtrim(std::string s);
Value instr(std::vector<Value> args);


} // namespace wwivbasic::stdlib
