#include <boost/preprocessor/cat.hpp>
#include <stdexcept>


namespace rvn::exception
{

class parsing_exception : public std::runtime_error
{
public:
    // TODO: update to also store type which couldn't get parsed
    parsing_exception() : std::runtime_error("Failed to parse")
    {
    }
};
} // namespace rvn::exception
