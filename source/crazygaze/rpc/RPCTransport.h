#pragma once

namespace cz
{
namespace rpc
{

class Transport : public std::enable_shared_from_this<Transport>
{
  public:
	virtual ~Transport() {}

	// Send one single RPC
	// \return
	//	false if the transport is closed
	//	true if send was successful. (Doesn't necessarily mean it will reach the destination)
	virtual bool send(std::vector<char> data) = 0;

	// Receive one single RPC
	// dst : Will contain the data for one single RPC, or empty if no RPC available
	// return: true if the transport is still alive, false if the transport closed
	virtual bool receive(std::vector<char>& dst) = 0;

	// Close connection to the peer
	virtual void close() = 0;

};
}
}

