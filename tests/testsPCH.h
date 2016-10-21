#pragma once

#ifdef _WIN32
#include "targetver.h"
#endif

//
// czrpc
//
#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCAsioTransport.h"
#include "crazygaze/rpc/RPCTCPSocketTransport.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <queue>
#include <cstdarg>

//
// UnitTest++
//
#include "UnitTest++/UnitTest++.h"
#include "UnitTest++/CurrentTest.h"

#include "Semaphore.h"

