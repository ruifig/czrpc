#pragma once


#define CZRPC_WINSOCK 1
//#define CZRPC_BSD 2

//
// Excellent BSD socket tutorial:
// http://beej.us/guide/bgnet/
//
// Compatibility stuff (Windows vs Unix)
// https://tangentsoft.net/wskfaq/articles/bsd-compatibility.html

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <strsafe.h>

#define TCPLOG(fmt, ...) printf(fmt##"\n", ##__VA_ARGS__)

namespace cz
{
namespace rpc
{
namespace details
{
	struct TCPUtils
	{
		static std::string getWin32ErrorMsg(const char* funcname = nullptr)
		{
			LPVOID lpMsgBuf;
			LPVOID lpDisplayBuf;
			DWORD dw = GetLastError();

			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(char*)&lpMsgBuf,
				0,
				NULL);

			int funcnameLength = funcname ? (int)strlen(funcname) : 0;
			lpDisplayBuf =
				(LPVOID)LocalAlloc(LMEM_ZEROINIT, (strlen((char*)lpMsgBuf) + funcnameLength + 50));
			StringCchPrintfA(
				(char*)lpDisplayBuf,
				LocalSize(lpDisplayBuf),
				"%s failed with error %d: %s",
				funcname ? funcname : "",
				dw,
				lpMsgBuf);

			std::string ret = (char*)lpDisplayBuf;
			LocalFree(lpMsgBuf);
			LocalFree(lpDisplayBuf);

			// Remove the \r\n at the end
			while (ret.size() && ret.back() < ' ')
				ret.pop_back();

			return std::move(ret);
		}

		struct WSAInstance
		{
			WSAInstance()
			{
				WORD wVersionRequested = MAKEWORD(2, 2);
				WSADATA wsaData;
				int err = WSAStartup(wVersionRequested, &wsaData);
				if (err != 0)
					throw std::runtime_error(getWin32ErrorMsg());
				if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
				{
					WSACleanup();
					throw std::runtime_error("Could not find a usable version of Winsock.dll");
				}
			}
			~WSAInstance()
			{
				WSACleanup();
			}
		};

		static void setBlocking(SOCKET s, bool blocking)
		{
			assert(s != INVALID_SOCKET);
			// 0: Blocking. !=0 : Non-blocking
			u_long mode = blocking ? 0 : 1;
			int res = ioctlsocket(s, FIONBIO, &mode);
			if (res != 0)
				throw std::runtime_error(getWin32ErrorMsg().c_str());
		}

		static void disableNagle(SOCKET s)
		{
			int flag = 1;
			int result = setsockopt(
				s, /* socket affected */
				IPPROTO_TCP,     /* set option at TCP level */
				TCP_NODELAY,     /* name of option */
				(char *)&flag,  /* the cast is historical cruft */
				sizeof(flag));    /* length of option value */
			if (result !=0)
				throw std::runtime_error(getWin32ErrorMsg().c_str());
		}

		static std::pair<std::string, int> addrToPair(sockaddr_in& addr)
		{
			std::pair<std::string, int> res;
			char str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(addr.sin_addr), str, INET_ADDRSTRLEN);
			res.first = str;
			res.second = ntohs(addr.sin_port);
			return res;
		}

		static std::pair<std::string, int> getLocalAddr(SOCKET s)
		{
			sockaddr_in addr;
			int size = sizeof(addr);
			if (getsockname(s, (SOCKADDR*)&addr, &size) != SOCKET_ERROR && size == sizeof(addr))
				return addrToPair(addr);
			else
				return std::make_pair("", 0);
		}

		static std::pair<std::string, int> getRemoteAddr(SOCKET s)
		{
			sockaddr_in addr;
			int size = sizeof(addr);
			if (getpeername(s, (SOCKADDR*)&addr, &size) != SOCKET_ERROR)
				return addrToPair(addr);
			else
				return std::make_pair("", 0);
		}
	};
}

class TCPSocketSet;

class BaseSocket : public std::enable_shared_from_this<BaseSocket> 
{
public:
	BaseSocket(bool isListenSocket)
		: m_isListenSocket(isListenSocket) {}
	virtual ~BaseSocket()
	{
		TCPLOG("Destroying %d", (int)m_s);
		::closesocket(m_s);
	}
protected:
	friend class TCPSocketSet;
	bool m_isListenSocket;
	TCPSocketSet* m_owner = nullptr;
	SOCKET m_s = INVALID_SOCKET;
};

class TCPSocket : public BaseSocket
{
public:
	TCPSocket() : BaseSocket(false) {}
	virtual ~TCPSocket() { }

	void send(std::vector<char> data);
	void send(char* buf, int len);

	//! Sets the socket as stopped.
	// A stopped socket behaves as follow:
	// - Any future send calls will be ignored
	// - No more data will be received
	// - It will be automatically removed from the owning TCPSocketSet when all the currently pending outgoing data is
	// sent or an error occurs.
	void stop();
	
	//! WARNING: This is not thread safe.
	// It should only be called from inside TCPSocketSet::tick, such as when executing the
	// callback for a TCPSocketSet::listen
	void setOnRecv(std::function<void(const char*, int)> onRecv);

protected:
	std::shared_ptr<TCPSocket> shared_from_this()
	{
		return std::static_pointer_cast<TCPSocket>(BaseSocket::shared_from_this());
	}
	friend class TCPSocketSet;
	std::pair<std::string, int> m_localAddr;
	std::pair<std::string, int> m_peerAddr;
	std::function<void(const char*, int)> m_onRecv;
	std::queue<std::vector<char>> m_out;
	int m_currOutOffset = 0;

};

class TCPListenSocket : public BaseSocket
{
public:
	TCPListenSocket() : BaseSocket(true) {}
	virtual ~TCPListenSocket() { }
private:
	std::shared_ptr<TCPListenSocket> shared_from_this()
	{
		return std::static_pointer_cast<TCPListenSocket>(BaseSocket::shared_from_this());
	}
	friend class TCPSocketSet;
	std::function<void(std::shared_ptr<TCPSocket> s)> onAccept;
	std::pair<std::string, int> m_localAddr;
};

class TCPSocketSet
{
public:

	TCPSocketSet()
	{
		auto dummyListen = listen(0,
			[this](std::shared_ptr<TCPSocket> s)
		{
			m_signalIn = s;
			TCPLOG("m_signalIn socket=%d", (int)(m_signalIn->m_s));
		});

		TCPLOG("Dummy listen socket=%d", (int)(dummyListen->m_s));
		auto tmptick = std::async(std::launch::async, [this]
		{
			while (!m_signalIn)
				tick();
		});
		
		m_signalOut = connect("127.0.0.1", dummyListen->m_localAddr.second, nullptr);
		TCPLOG("m_signalOut socket=%d", (int)(m_signalOut->m_s));
		tmptick.get();

		// Remove the outgoing dummy TCP sockets from the map, since we don't need to track it with select.
		// We write data to it explicitly as a trigger to break the select
		remove(m_signalOut->m_s);
		dummyListen = nullptr; // Not needed, since it goes out of scope, but easier to debug

		details::TCPUtils::disableNagle(m_signalIn->m_s);
		details::TCPUtils::disableNagle(m_signalOut->m_s);
	}

	std::shared_ptr<TCPSocket> connect(const char* ip, int port, std::function<void(const char*, int)> onRecv)
	{
		SOCKADDR_IN addr;
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &(addr.sin_addr));

		auto sock = std::make_shared<TCPSocket>();
		sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock->m_s == INVALID_SOCKET)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg(__FUNCTION__));

		TCPLOG("Connect socket=%d", (int)sock->m_s);

		if (::connect(sock->m_s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg(__FUNCTION__));

		details::TCPUtils::setBlocking(sock->m_s, false);

		sock->m_owner = this;
		sock->m_localAddr = details::TCPUtils::getLocalAddr(sock->m_s);
		sock->m_peerAddr = details::TCPUtils::getRemoteAddr(sock->m_s);
		sock->setOnRecv(std::move(onRecv));
		TCPLOG("Socket %d connected to %s:%d", (int)sock->m_s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second);

		m_data([this, &sock](Data& data)
		{
			data.sockets.push_back(sock);
		});

		return sock;
	}

	std::shared_ptr<TCPListenSocket> listen(int port, std::function<void(std::shared_ptr<TCPSocket>)> onAccept)
	{
		auto sock = std::make_shared<TCPListenSocket>();
		sock->m_owner = this;
		sock->onAccept = std::move(onAccept);

		sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock->m_s == INVALID_SOCKET)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg(__FUNCTION__));
		//details::TCPUtils::setBlocking(sock->m_s, false);

		TCPLOG("Listen socket=%d", (int)sock->m_s);

		SOCKADDR_IN addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (::bind(sock->m_s, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg(__FUNCTION__));

		if (::listen(sock->m_s, SOMAXCONN) == SOCKET_ERROR)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg(__FUNCTION__));

		sock->m_localAddr = details::TCPUtils::getLocalAddr(sock->m_s);

		m_data([this, &sock](Data& data)
		{
			data.sockets.push_back(sock);
		});

		return sock;
	}

	bool tick()
	{
		if (m_running)
			tickImpl();
		return m_running;
	}

	//! Sets the set as stopped, making any current tick call exit and return false.
	// Any other tick calls will return immediately with false
	void stop()
	{
		m_running = false;
		this->signal();
	}

protected:

	using WorkQueue = std::queue<std::function<void()>>;
	Monitor<WorkQueue> m_work;

	template<typename F>
	void addWork(F&& f)
	{
		m_work([&](WorkQueue& q)
		{
			q.push(std::move(f));
		});
	}

	void tickImpl()
	{
		fd_set readfds, writefds;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		SOCKET maxfd = 0;
		
		// #TODO : Remove these. Used for debugging
		std::string rstr, wstr;

		m_data([&](Data& data)
		{
			for(auto it = data.sockets.begin(); it!=data.sockets.end(); ++it)
			{
				const SOCKET s = (*it)->m_s;
				if (s > maxfd)
					maxfd = s;
				FD_SET(s, &readfds);
				rstr += std::to_string((int)s) + ", ";

				if (!(*it)->m_isListenSocket)
				{
					auto sock = static_cast<TCPSocket*>(it->get());
					if (sock->m_outSize > 0)
					{
						FD_SET(s, &writefds);
						wstr += std::to_string((int)s) + ", ";
					}
				}
			}
		});

		if (readfds.fd_count == 0 && writefds.fd_count == 0)
			return;

		TCPLOG("SELECT %d, readCount=%d(%s), writeCount=%d(%s)",
			(int)maxfd + 1, readfds.fd_count, rstr.c_str(), writefds.fd_count, wstr.c_str());
		auto res = select((int)maxfd + 1, &readfds, &writefds, NULL, NULL);
		TCPLOG("    SELECT RES=%d", res);
		if (res== SOCKET_ERROR)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg());

		if (res == 0)
			return;

		m_data([&](Data& data)
		{
			for(auto it = data.sockets.begin(); it!=data.sockets.end(); )
			{
				if (FD_ISSET((*it)->m_s, &readfds))
				{
					if (p.second->m_isListenSocket)
						processAccept(data, static_cast<TCPListenSocket*>(it->get()));
					else
						processRead(static_cast<TCPSocket*>(it->get()));
				}
				else if (FD_ISSET((*it)->m_s, &writefds))
				{
					if ((*it)->m_isListenSocket)
					{
						// This should not happen?
						assert(0);
					}
					else
						processWrite(static_cast<TCPSocket*>(it->get()));
				}

				//
				// Remove any unneeded sockets from the set
				//	- Sockets marked as stopped and have no pending outgoing data
				//	- Sockets that have no external references and have no pending outgoing data
				bool remove = false;
				if ((*it)->m_isListenSocket)
				{
					if (it->unique())
						remove = true;
				}
				else
				{
					auto sock = static_cast<TCPSocket*>(it->get());
					if ((sock->isStopped() || it->unique()) && sock->m_outSize == 0)
						remove = true;
				}

				if (remove)
					it = data.sockets.erase(it);
				else
					++it;
			}
		});
	}

	struct Data
	{
		std::vector<std::shared_ptr<BaseSocket>> sockets;
	};
	Monitor<Data> m_data;
	details::TCPUtils::WSAInstance m_wsaInstance;
	bool m_running = true;
	void processAccept(Data& data, TCPListenSocket* sock)
	{
		sockaddr_in clientAddr;
		int size = sizeof(clientAddr);
		SOCKET s = ::accept(sock->m_s, (struct sockaddr*)&clientAddr, &size);
		if (s == INVALID_SOCKET)
			return;

		auto client = std::make_shared<TCPSocket>();
		client->m_s = s;
		client->m_localAddr = details::TCPUtils::getLocalAddr(s);
		client->m_peerAddr = details::TCPUtils::getRemoteAddr(s);
		client->m_owner = this;
		details::TCPUtils::setBlocking(client->m_s, false);
		data.sockets[s] = client.get();
		TCPLOG("Server side socket %d connected to %s:%d, socket %d",
			(int)s, client->m_peerAddr.first.c_str(), client->m_peerAddr.second,
			(int)sock->m_s);
		sock->onAccept(std::move(client));
	}

	void processRead(TCPSocket* sock)
	{
		// Loop and keep reading as much data as we can
		while(true)
		{
			// #TODO : Race condition here between m_stopped and m_onRecv, since if we pass this check and
			// another thread then calls stop before we reach the use of m_onRecv, things can break.
			if (sock->m_stopped)
				return;
			int len = ::recv(sock->m_s, m_recvBuf, sizeof(m_recvBuf), 0);
			if (len == SOCKET_ERROR)
			{
#if CZRPC_WINSOCK
				auto err = WSAGetLastError();
				if (err == WSAEWOULDBLOCK)
					return;
#elif CZRPC_BSD
				auto err = errno;
				if (err == EAGAIN || err == EWOULDBLOCK)
					return;
#else
	#error "Unknown"
#endif
				else
					throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg());
			}

			if (sock->m_onRecv)
				sock->m_onRecv((len) ? m_recvBuf : nullptr, len);
		}
	}

	void processWrite(TCPSocket* sock)
	{
		while(true)
		{
			if (sock->m_outSize == 0)
				return;

			// Grab another non-empty chunk from the queue if necessary
			while(sock->m_currOut.size()==0)
			{
				sock->m_out([sock](TCPSocket::Out& out)
				{
					assert(out.q.size());
					sock->m_currOut = std::move(out.q.front());
					out.q.pop();
				});
			}

			int todo = (int)sock->m_currOut.size() - sock->m_currOutOffset;
			auto res = ::send(sock->m_s, sock->m_currOut.data() + sock->m_currOutOffset, todo, 0);

			if (res==todo)
			{
				// All the data was passed on to the transport system, so we can loop and try to send more
				sock->m_currOut.clear();
				sock->m_currOutOffset = 0;
				sock->m_outSize -= res;
			}
			else if (res>0)
			{
				// Data was partially sent, so that means the transport system buffer is full, and
				// we should not try to keep sending
				sock->m_currOutOffset += res;
				sock->m_outSize -= res;
				assert(sock->m_currOutOffset < (int)sock->m_currOut.size());
				return;
			}
			else if (res==-1)
			{
				// #TODO : Need to test this
				if (errno == EWOULDBLOCK)
					return;
			}
			else
			{
				// We should not get here
				assert(0);
			}
		}
	}

	void remove(SOCKET s)
	{
		TCPLOG("Removing %d", (int)s);
		m_data([this, s](Data& data)
		{
			auto it = data.sockets.find(s);
			if (it == data.sockets.end())
			{
				TCPLOG("    %d not in the set", (int)s);
				return;
			}
			data.sockets.erase(it);
		});
	}

	void signal()
	{
		char buf = 0;
		if (::send(m_signalOut->m_s, &buf, 1, 0)!=1)
			throw std::runtime_error(details::TCPUtils::getWin32ErrorMsg());
	}

	friend class BaseSocket;
	friend class TCPSocket;
	friend class TCPListenSocket;

	// To interrupt the select call, we use dummy TCP sockets.
	// We send 1 byte each time we want to interrupt a select
	std::shared_ptr<TCPSocket> m_signalIn;
	std::shared_ptr<TCPSocket> m_signalOut;
	char m_recvBuf[64000];
};

void TCPSocket::send(std::vector<char> data)
{
	TCPLOG("Sending %d bytes", (int)data.size());
	m_owner->addWork([data=std::move(data), this_=shared_from_this()]
	{
		this_->m_out.push(std::move(data));
	});
	m_owner->signal();
}

void TCPSocket::send(char* buf, int len)
{
	send(std::vector<char>(buf, buf + len));
}

void TCPSocket::setOnRecv(std::function<void(const char*, int)> onRecv)
{
	m_owner->addWork([onRecv=std::move(onRecv), this_=shared_from_this()]
	{
		this_->m_onRecv = std::move(onRecv);
	});
	m_owner->signal();
}

} // namespace rpc
} // namespace cz

