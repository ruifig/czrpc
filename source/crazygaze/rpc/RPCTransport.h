#pragma once

namespace cz
{
namespace rpc
{

class SessionData
{
public:
	virtual ~SessionData() {}
};

// Forward declaration
class BaseConnection;

class Transport
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

	//! Called by the Connection when new data is ready to send
	// This makes it possible for the transport to be the one triggering the call to Connection::process 
	virtual void onSendReady() {}

protected:
	friend BaseConnection;
	BaseConnection* m_con = nullptr;
};
}
}

