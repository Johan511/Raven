#include <msquic.h>
///////////////////////////////////////
#include <contexts.hpp>
#include <protobuf_messages.hpp>
#include <serialization.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
///////////////////////////////////////

namespace rvn::utils
{

class MOQTComplexGetterUtils
{
    friend class MOQT;
    MOQT& moqt;

    MOQTComplexGetterUtils(MOQT* moqt) : moqt(*moqt)
    {
    }

public:
};

} // namespace rvn::utils
