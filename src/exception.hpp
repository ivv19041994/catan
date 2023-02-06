#pragma once

#include <string>
#include <stdexcept>

namespace ivv {
namespace catan {

class logic_error : public std::logic_error {
public:
	logic_error(const std::string& what);
};

class out_of_range : public std::out_of_range
{
public:
	out_of_range(const std::string& what);
};

class invalid_argument : public std::invalid_argument
{
public:
	invalid_argument(const std::string& what);
};
}//namespace ivv::catan {
}//namespace ivv {