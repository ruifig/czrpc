#pragma once

//
// Utility server class that allows clients to connect to any server type,
// if they only want to call generic RPCs.
//

namespace cz
{
namespace rpc
{

//
// Its intentionally empty, since the only RPC that will be available is "genericRPC",
// and that is implemented by the rest of the framework.
class GenericServer
{
public:
};

} // namespace cz
} // namespace cz


#define RPCTABLE_CLASS cz::rpc::GenericServer
#define RPCTABLE_CONTENTS
#include "crazygaze/rpc/RPCGenerate.h"
