#pragma once

#ifdef _WIN32
#include "targetver.h"
#include <tchar.h>
#endif

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCSpasTransport.h"
#include "../Common/StringUtil.h"
#include "../Common/Misc.h"

CZRPC_ALLOW_RVALUE_REFS;


