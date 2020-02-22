#include "testsPCH.h"
#include "Foo.h"

std::atomic<int> Foo::ms_defaultCalls(0);
std::atomic<int> Foo::ms_moveCalls(0);
std::atomic<int> Foo::ms_copyCalls(0);

