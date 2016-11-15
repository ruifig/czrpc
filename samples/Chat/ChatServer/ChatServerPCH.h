#pragma once

#ifdef _WIN32
#include "targetver.h"
#include <tchar.h>
#endif

#include <stdio.h>
#include <string>
#include <algorithm>

#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCAsioTransport.h"
#include "../../SamplesCommon/Parameters.h"
#include "../../SamplesCommon/StringUtil.h"
#include "../../SamplesCommon/Misc.h"
#include "../ChatCommon/ChatInterface.h"