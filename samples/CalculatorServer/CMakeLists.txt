cmake_minimum_required(VERSION 3.16)

setup_czrpc_sample(CalculatorServer
		"CalculatorInterface.h"
		"CalculatorServer.cpp"
		"targetver.h"
		"CalculatorServerPCH.h"
		)

