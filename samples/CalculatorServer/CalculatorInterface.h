#pragma once
#include <future>

//
// A simple example on how to split the interface and implementation of a server, so the client
// doesn't need to link with the implementation.
// The client only needs to know the interface.

struct CalculatorInterface
{
	virtual float add(float a, float b) = 0;
	virtual float sub(float a, float b) = 0;
	
	// Calculates the square roots of a number
	// For testing, it returns a future, as an example of an asynchronous server side API
	virtual std::future<float> sqrt(float a) = 0;
};

//
// Specify the RPC table
#define RPCTABLE_CLASS CalculatorInterface
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add) \
	REGISTERRPC(sub) \
	REGISTERRPC(sqrt)
#include "crazygaze/rpc/RPCGenerate.h"
	