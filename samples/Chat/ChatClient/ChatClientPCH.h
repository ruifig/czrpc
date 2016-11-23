#pragma once

#ifdef _WIN32
#include "targetver.h"
#include <tchar.h>
#endif

#include <stdio.h>
#include <iostream>

#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCTCPSocketTransport.h"
#include "../ChatCommon/ChatInterface.h"
#include "../../SamplesCommon/StringUtil.h"
#include "../../SamplesCommon/Parameters.h"