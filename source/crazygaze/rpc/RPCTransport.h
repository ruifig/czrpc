#pragma once

namespace cz
{
namespace rpc
{

class Transport
{
public:
	virtual ~Transport() {}

	//! Send one single RPC
	virtual void send(std::vector<char> data) = 0;

	//! Receive one single RPC
	// \param dst
	//	Data for 1 RPC, or empty if no RPC available
	// \return true if the transport is still alive, false if the transport is dead/closed
	virtual bool receive(std::vector<char>& dst) = 0;

	//! Close connection to the peer
	virtual void close() = 0;
};

}
}

