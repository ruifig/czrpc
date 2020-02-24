#pragma once

#ifdef _WIN32
#include "targetver.h"
#include <tchar.h>
#endif

#include <stdio.h>
#include <string>
#include <algorithm>

#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCSpasTransport.h"
#include "../../Common/Parameters.h"
#include "../../Common/StringUtil.h"
#include "../../Common/Misc.h"
#include "../ChatCommon/ChatInterface.h"
