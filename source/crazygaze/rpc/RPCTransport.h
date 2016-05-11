#pragma once

namespace cz
{
namespace rpc
{

class Transport
{
public:
	//! Send 1 single RPC
	virtual void send(std::vector<char> data) = 0;

	//! Receive 1 single RPC
	// \param dst
	//	If an rpc was received, this contains the RPC data.
	//	Any existing data is cleared (if there is an RPC or not)
	// \return true if RPC received, false if no RPC
	virtual bool receive(std::vector<char>& dst) = 0;
};

}
}

