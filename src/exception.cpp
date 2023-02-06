#include "exception.hpp"

namespace ivv {
namespace catan {


logic_error::logic_error(const std::string& what) : std::logic_error{ what }
{

}

out_of_range::out_of_range(const std::string& what) : std::out_of_range{ what }
{

}

invalid_argument::invalid_argument(const std::string& what) : std::invalid_argument{ what }
{

}
}//namespace ivv::catan {
}//namespace ivv {