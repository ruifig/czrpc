//
// Excellent BSD socket tutorial:
// http://beej.us/guide/bgnet/
//
// Compatibility stuff (Windows vs Unix)
// https://tangentsoft.net/wskfaq/articles/bsd-compatibility.html
//
// Nice question/answer about socket states, including a neat state diagram:
//	http://stackoverflow.com/questions/5328155/preventing-fin-wait2-when-closing-socket

#pragma once


#ifdef _WIN32
	#define CZRPC_WINSOCK 1
	#include <WinSock2.h>
	#include <WS2tcpip.h>
#endif

#include <strsafe.h>
#include <set>
#include <stdio.h>

namespace cz
{
namespace rpc
{

struct TCPSocketDefaultLog
{
	static void out(bool fatal, const char* type, const char* fmt, ...)
	{
		char buf[32];
		strncpy(buf, type, sizeof(buf));
		buf[sizeof(buf)-1] = 0;
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1, fmt, args);
		va_end(args);
		printf(buf);
		printf("\n");
		if (fatal)
		{
			__debugbreak();
			exit(1);
		}
	}
};

#ifndef TCPINFO
	#define TCPINFO(fmt, ...) TCPSocketDefaultLog::out(false, "Info: ", fmt, ##__VA_ARGS__)
#endif
#ifndef TCPWARNING
	#define TCPWARN(fmt, ...) TCPSocketDefaultLog::out(false, "Warning: ", fmt, ##__VA_ARGS__)
#endif
#ifndef TCPERROR
	#define TCPERROR(fmt, ...) TCPSocketDefaultLog::out(false, "Error: ", fmt, ##__VA_ARGS__)
#endif
#ifndef TCPFATAL
	#define TCPFATAL(fmt, ...) TCPSocketDefaultLog::out(true, "Fatal: ", fmt, ##__VA_ARGS__)
#endif

#ifndef TCPASSERT
	#define TCPASSERT(expr) \
		if (!(expr)) TCPFATAL(#expr)
#endif

/*! Utility buffer struct
Simply ties together shared raw buffer pointer and its size
This makes it for shorter code when passing it around in lambdas such as for
receiving and sending data.
*/
struct TCPBuffer
{
	TCPBuffer(int size) : size(size)
	{
		// Allocate with custom deleter
		buf = std::shared_ptr<char>(new char[size], [](char* p) { delete[] p;});
	}
	char* ptr() { return buf.get(); }
	const char* ptr() const { return buf.get(); }

	void zero()
	{
		memset(ptr(), 0, size);
	}

	std::shared_ptr<char> buf;
	int size;
};

struct TCPError
{
	enum class Code
	{
		Success,
		Cancelled,
		ConnectionClosed,
		Timeout,
		Other
	};

	TCPError(Code c = Code::Success) : code(c) {}
	TCPError(Code c, const char* msg) : code(c)
	{
		setMsg(msg);
	}
	TCPError(Code c, const std::string& msg) : code(c)
	{
		setMsg(msg.c_str());
	}

	const char* msg() const
	{
		if (optionalMsg)
			return optionalMsg->c_str();
		switch (code)
		{
			case Code::Success: return "Success";
			case Code::Cancelled: return "Cancelled";
			case Code::ConnectionClosed: return "ConnectionClosed";
			case Code::Timeout: return "Timeout";
			default: return "Unknown";
		}
	}
	
	void setMsg(const char* msg)
	{
		// Always create a new one, since it might be shared by other instances
		optionalMsg = std::make_shared<std::string>(msg);
	}

	//! Check if there is an error
	// Note that it return true IF THERE IS AN ERROR, not the other way around.
	// This makes for shorter code
	operator bool() const
	{
		return code != Code::Success;
	}

	Code code;
	std::shared_ptr<std::string> optionalMsg;
};

class TCPAcceptor;
class TCPSocket;
class TCPService;

namespace details
{
	class ErrorWrapper
	{
	public:
#if CZRPC_WINSOCK
		static std::string getWin32ErrorMsg(DWORD err = ERROR_SUCCESS, const char* funcname = nullptr)
		{
			LPVOID lpMsgBuf;
			LPVOID lpDisplayBuf;
			if (err == ERROR_SUCCESS)
				err = GetLastError();

			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				err,
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
				err,
				lpMsgBuf);

			std::string ret = (char*)lpDisplayBuf;
			LocalFree(lpMsgBuf);
			LocalFree(lpDisplayBuf);

			// Remove the \r\n at the end
			while (ret.size() && ret.back() < ' ')
				ret.pop_back();

			return std::move(ret);
		}

		ErrorWrapper() { err = WSAGetLastError(); }
		std::string msg() const { return getWin32ErrorMsg(err); }
		bool isBlockError() const { return err == WSAEWOULDBLOCK; }
#else
		ErrorWrapper() { err = errno; }
		bool isBlockError() const { return err == EAGAIN || err == EWOULDBLOCK || EINPROGRESS; }
		// #TODO Build custom error depending on the error number
		std::string msg() const { return "Error " + std::to_string(err); }
#endif

		TCPError getError() const { return TCPError(TCPError::Code::Other, msg()); }
	private:
		int err;
	};

	struct utils
	{
		static void setBlocking(SOCKET s, bool blocking)
		{
			TCPASSERT(s != INVALID_SOCKET);
			// 0: Blocking. !=0 : Non-blocking
			u_long mode = blocking ? 0 : 1;
			int res = ioctlsocket(s, FIONBIO, &mode);
			if (res != 0)
				TCPFATAL(ErrorWrapper().msg().c_str());
		}

		static void disableNagle(SOCKET s)
		{
			int flag = 1;
			int result = setsockopt(
				s, /* socket affected */
				IPPROTO_TCP,     /* set option at TCP level */
				TCP_NODELAY,     /* name of option */
				(char *)&flag,   /* the cast is historical cruft */
				sizeof(flag));   /* length of option value */
			if (result != 0)
				TCPFATAL(ErrorWrapper().msg().c_str());
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
			{
				TCPFATAL(ErrorWrapper().msg().c_str());
				return std::make_pair("", 0);
			}
		}

		static std::pair<std::string, int> getRemoteAddr(SOCKET s)
		{
			sockaddr_in addr;
			int size = sizeof(addr);
			if (getpeername(s, (SOCKADDR*)&addr, &size) != SOCKET_ERROR)
				return addrToPair(addr);
			else
			{
				TCPFATAL(ErrorWrapper().msg().c_str());
				return std::make_pair("", 0);
			}
		}
	};

	struct WSAInstance
	{
		WSAInstance()
		{
			WORD wVersionRequested = MAKEWORD(2, 2);
			WSADATA wsaData;
			int err = WSAStartup(wVersionRequested, &wsaData);
			if (err != 0)
				TCPFATAL(ErrorWrapper().msg().c_str());

			if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
			{
				WSACleanup();
				TCPFATAL("Could not find a usable version of Winsock.dll");
			}
		}
		~WSAInstance()
		{
			WSACleanup();
		}
	};

	struct TCPServiceData;
	//////////////////////////////////////////////////////////////////////////
	// TCPBaseSocket
	//////////////////////////////////////////////////////////////////////////
	class TCPBaseSocket : public std::enable_shared_from_this<TCPBaseSocket>
	{
	public:
		TCPBaseSocket() {}
		virtual ~TCPBaseSocket()
		{
			::shutdown(m_s, SD_BOTH);
			::closesocket(m_s);
		}
	protected:
		friend TCPServiceData;
		friend TCPService;
		SOCKET m_s = INVALID_SOCKET;
	};

	// This is not part of the TCPService class, so we can break the circular dependency
	// between TCPAcceptor/TCPSocket and TCPService
	struct TCPServiceData
	{
		TCPServiceData() : m_stopped(false) { }
		details::WSAInstance m_wsaInstance;
		std::shared_ptr<TCPBaseSocket> m_signalIn;
		std::shared_ptr<TCPBaseSocket> m_signalOut;

		std::atomic<bool> m_stopped;

		using ConnectHandler = std::function<void(const TCPError&, std::shared_ptr<TCPSocket>)>;
		struct ConnectOp
		{
			std::chrono::time_point<std::chrono::high_resolution_clock> timeoutPoint;
			ConnectHandler h;
		};

		std::unordered_map<std::shared_ptr<TCPSocket>, ConnectOp> m_connects;  // Pending connects
		std::set<std::shared_ptr<TCPAcceptor>> m_accepts; // pending accepts
		std::set<std::shared_ptr<TCPSocket>> m_recvs; // pending reads
		std::set<std::shared_ptr<TCPSocket>> m_sends; // pending writes

		using CmdQueue = std::queue<std::function<void()>>;
		Monitor<CmdQueue> m_cmdQueue;
		CmdQueue m_tmpQueue;
		char m_signalInBuf[512];
		void signal()
		{
			if (!m_signalOut)
				return;
			char buf = 0;
			if (::send(m_signalOut->m_s, &buf, 1, 0) != 1)
				TCPFATAL(details::ErrorWrapper().msg().c_str());
		}

		template<typename F>
		void addCmd(F&& f)
		{
			m_cmdQueue([&](CmdQueue& q)
			{
				q.push(std::move(f));
			});
			signal();
		}

	};
} // namespace details


//////////////////////////////////////////////////////////////////////////
// TCPSocket
//////////////////////////////////////////////////////////////////////////
/*!
Main socket class, used to send and receive data

Thread Safety:
	Distinct objects  : Safe
	Shared objects : Unsafe
*/
class TCPSocket : public details::TCPBaseSocket
{
public:
	using Handler = std::function<void(const TCPError& ec, int bytesTransfered)>;

	virtual ~TCPSocket()
	{
		TCPASSERT(m_recvs.size() == 0);
		TCPASSERT(m_sends.size() == 0);
	}

	//
	// Asynchronous reading 
	//
	template<typename H>
	void asyncRecv(char* buf, int len, H&& h)
	{
		asyncRecvImpl(buf, len, std::forward<H>(h), false);
	}
	template<typename H>
	void asyncRecv(TCPBuffer& buf, H&& h)
	{
		asyncRecvImpl(buf.buf.get(), buf.size, std::forward<H>(h), false);
	}
	template<typename H>
	void asyncRecvFull(char* buf, int len, H&& h)
	{
		asyncRecvImpl(buf, len, std::forward<H>(h), true);
	}
	template<typename H>
	void asyncRecvFull(TCPBuffer& buf, H&& h)
	{
		asyncRecvImpl(buf.buf.get(), buf.size, std::forward<H>(h), true);
	}

	//
	// Asynchronous sending
	//
	void asyncSend(const char* buf, int len, Handler h)
	{
		SendOp op;
		op.buf = buf;
		op.bufLen = len;
		op.h = std::move(h);
		m_owner->addCmd([this_ = shared_from_this(), op = std::move(op)]
		{
			this_->m_sends.push(std::move(op));
			this_->m_owner->m_sends.insert(this_);
		});
	}

	template<typename H>
	void asyncSend(const TCPBuffer& buf, H&& h)
	{
		asyncSend(buf.buf.get(), buf.size, std::forward<H>(h));
	}

	//! Cancels all outstanding asynchronous operations
	void cancel()
	{
		m_owner->addCmd([this_ = shared_from_this()]
		{
			this_->doCancel();
			this_->m_owner->m_recvs.erase(this_);
			this_->m_owner->m_sends.erase(this_);
		});
	}

	const std::pair<std::string, int>& getLocalAddress() const
	{
		return m_localAddr;
	}

	const std::pair<std::string, int>& getPeerAddress() const
	{
		return m_peerAddr;
	}

	std::shared_ptr<TCPSocket> shared_from_this()
	{
		return std::static_pointer_cast<TCPSocket>(TCPBaseSocket::shared_from_this());
	}

protected:
	void asyncRecvImpl(char* buf, int len, Handler h, bool fill)
	{
		RecvOp op;
		op.buf = buf;
		op.bufLen = len;
		op.fill = fill;
		op.h = std::move(h);
		m_owner->addCmd([this_ = shared_from_this(), op = std::move(op)]
		{
			this_->m_recvs.push(std::move(op));
			this_->m_owner->m_recvs.insert(this_);
		});
	}

	struct RecvOp
	{
		char* buf = nullptr;
		int bufLen = 0;
		// If true, it will keep reading into this operation, until the specified buffer is full.
		// Only then the handler is called, and the operation discarded;
		// If false, the handler will be called with whatever data is received (even if less than the
		// buffer size, and the operation discarded;
		bool fill = false;
		int bytesTransfered = 0;
		Handler h;
	};
	struct SendOp
	{
		const char* buf = nullptr;
		int bufLen = 0;
		int bytesTransfered = 0;
		Handler h;
	};

	friend TCPService;
	friend TCPAcceptor;
	details::TCPServiceData* m_owner = nullptr;
	std::pair<std::string, int> m_localAddr;
	std::pair<std::string, int> m_peerAddr;

	bool doRecv()
	{
		TCPASSERT(m_recvs.size());

		while (m_recvs.size())
		{
			RecvOp& op = m_recvs.front();
			int len = ::recv(m_s, op.buf + op.bytesTransfered, op.bufLen - op.bytesTransfered, 0);
			if (len == SOCKET_ERROR)
			{
				details::ErrorWrapper err;
				if (err.isBlockError())
				{
					TCPASSERT(op.bytesTransfered);
					// If this operation doesn't require a full buffer, we call the handler with
					// whatever data we received, and discard the operation

					if (!op.fill)
					{
						op.h(TCPError(), op.bytesTransfered);
						m_recvs.pop();
					}
					// Done receiving, since the socket doesn't have more incoming data
					break;
				}
				else
				{
					TCPERROR(err.msg().c_str());
				}
			}
			else if (len > 0)
			{
				op.bytesTransfered += len;
				if (op.bufLen == op.bytesTransfered)
				{
					op.h(TCPError(), op.bytesTransfered);
					m_recvs.pop();
				}
			}
			else if (len == 0)
			{
				// #TODO : Peer disconnected gracefully.
				// #TODO : Cancel all pending receives?
				op.h(TCPError(TCPError::Code::ConnectionClosed), op.bytesTransfered);
				m_recvs.pop();
				break;
			}
			else
			{
				TCPASSERT(0 && "This should not happen");
			}
		}

		return m_recvs.size() > 0;
	}

	bool doSend()
	{
		while (m_sends.size())
		{
			SendOp& op = m_sends.front();
			auto res = ::send(m_s, op.buf + op.bytesTransfered, op.bufLen - op.bytesTransfered, 0);
			if (res == SOCKET_ERROR)
			{
				details::ErrorWrapper err;
				if (err.isBlockError())
				{
					// We can't send more data at the moment, so we are done trying
					break;
				}
				else
				{
					TCPERROR(err.msg().c_str());
					if (op.h)
						op.h(TCPError(TCPError::Code::ConnectionClosed, err.msg()), op.bytesTransfered);
					m_sends.pop();
				}
			}
			else
			{
				op.bytesTransfered += res;
				if (op.bufLen == op.bytesTransfered)
				{
					if (op.h)
						op.h(TCPError(), op.bytesTransfered);
					m_sends.pop();
				}
			}
		}

		return m_sends.size() > 0;
	}

	void doCancel()
	{
		while (m_recvs.size())
		{
			m_recvs.front().h(TCPError::Code::Cancelled, m_recvs.front().bytesTransfered);
			m_recvs.pop();
		}

		while (m_sends.size())
		{
			m_sends.front().h(TCPError::Code::Cancelled, m_sends.front().bytesTransfered);
			m_sends.pop();
		}
	}

	std::queue<RecvOp> m_recvs;
	std::queue<SendOp> m_sends;
};

//////////////////////////////////////////////////////////////////////////
// TCPAcceptor
//////////////////////////////////////////////////////////////////////////
/*!
With TCPAcceptor you can listen for new connections on a specified port.
Thread Safety:
	Distinct objects  : Safe
	Shared objects : Unsafe
*/
class TCPAcceptor : public details::TCPBaseSocket
{
public:
	virtual ~TCPAcceptor()
	{
		TCPASSERT(m_accepts.size() == 0);
	}
	using AcceptHandler = std::function<void(const TCPError& ec, std::shared_ptr<TCPSocket> sock)>;

	//! Starts an asynchronous accept
	void accept(AcceptHandler h)
	{
		m_owner->addCmd([this_ = shared_from_this(), h(std::move(h))]
		{
			this_->m_accepts.push(std::move(h));
			this_->m_owner->m_accepts.insert(this_);
		});
	}

	//! Cancels all outstanding asynchronous operations
	void cancel()
	{
		m_owner->addCmd([this_ = shared_from_this()]
		{
			this_->doCancel();
			this_->m_owner->m_accepts.erase(this_);
		});
	}

	const std::pair<std::string, int>& getLocalAddress() const
	{
		return m_localAddr;
	}

	std::shared_ptr<TCPAcceptor> shared_from_this()
	{
		return std::static_pointer_cast<TCPAcceptor>(TCPBaseSocket::shared_from_this());
	}

protected:
	friend class TCPService;

	// Called from TCPSocketSet
	// Returns true if we still have pending accepts, false otherwise
	bool doAccept()
	{
		TCPASSERT(m_accepts.size());

		auto h = std::move(m_accepts.front());
		m_accepts.pop();

		sockaddr_in clientAddr;
		int size = sizeof(clientAddr);
		SOCKET s = ::accept(m_s, (struct sockaddr*)&clientAddr, &size);
		if (s == INVALID_SOCKET)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			h(ec, nullptr);
			return m_accepts.size() > 0;
		}

		auto sock = std::make_shared<TCPSocket>();
		sock->m_s = s;
		sock->m_localAddr = details::utils::getLocalAddr(s);
		sock->m_peerAddr = details::utils::getRemoteAddr(s);
		sock->m_owner = m_owner;
		details::utils::setBlocking(sock->m_s, false);
		TCPINFO("Server side socket %d connected to %s:%d, socket %d",
			(int)s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second,
			(int)m_s);
		h(TCPError(), sock);

		return m_accepts.size() > 0;
	}

	void doCancel()
	{
		while (m_accepts.size())
		{
			m_accepts.front()(TCPError::Code::Cancelled, nullptr);
			m_accepts.pop();
		}
	}

	details::TCPServiceData* m_owner = nullptr;
	std::queue<AcceptHandler> m_accepts;
	std::pair<std::string, int> m_localAddr;
};




//////////////////////////////////////////////////////////////////////////
// TCPService
//////////////////////////////////////////////////////////////////////////
/*
Thread Safety:
	Distinct objects  : Safe
	Shared objects : Unsafe
*/
class TCPService : public details::TCPServiceData
{
public:
	TCPService()
	{
		TCPError ec;
		auto dummyListen = listen(0, 1, ec);
		TCPASSERT(!ec);
		dummyListen->accept([this](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
		{
			TCPASSERT(!ec);
			m_signalIn = sock;
			TCPINFO("m_signalIn socket=%d", (int)(m_signalIn->m_s));
		});

		// Do this temporary ticking in a different thread, since our signal socket
		// is connected here synchronously
		TCPINFO("Dummy listen socket=%d", (int)(dummyListen->m_s));
		auto tmptick = std::async(std::launch::async, [this]
		{
			while (!m_signalIn)
				tick();
		});

		m_signalOut = connect("127.0.0.1", dummyListen->m_localAddr.second, ec);
		TCPASSERT(!ec);
		TCPINFO("m_signalOut socket=%d", (int)(m_signalOut->m_s));
		tmptick.get();

		// Initiate reading for the dummy input socket
		startSignalIn();

		dummyListen = nullptr; // Not needed, since it goes out of scope, but easier to debug

		details::utils::disableNagle(m_signalIn->m_s);
		details::utils::disableNagle(m_signalOut->m_s);
	}

	~TCPService()
	{
		TCPASSERT(m_stopped);
		static_cast<TCPSocket*>(m_signalOut.get())->doCancel();
		static_cast<TCPSocket*>(m_signalIn.get())->doCancel();
		m_cmdQueue([&](CmdQueue& q)
		{
			TCPASSERT(q.size() == 0);
		});
	}

	//! Creates a listening socket at the specified port
	/*
	This is a synchronous operation.
	\param port
		What port to listen on. If 0, the OS will pick a port from the dynamic range
	\param ec
		If an error occurs, this contains the error.
	\param backlog
		Size of the the connection backlog. Default value to use the maximum allowed.
		Also, this is only an hint to the OS. It's not guaranteed.
	\return
		The Acceptor socket, or nullptr, if there was an error
	*/
	std::shared_ptr<TCPAcceptor> listen(int port, int backlog, TCPError& ec)
	{
		auto sock = std::make_shared<TCPAcceptor>();
		sock->m_owner = this;

		sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock->m_s == INVALID_SOCKET)
		{
			ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return nullptr;
		}

		TCPINFO("Listen socket=%d", (int)sock->m_s);

		SOCKADDR_IN addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (::bind(sock->m_s, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return nullptr;
		}

		if (::listen(sock->m_s, backlog) == SOCKET_ERROR)
		{
			ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return nullptr;
		}

		sock->m_localAddr = details::utils::getLocalAddr(sock->m_s);

		ec = TCPError();
		return sock;
	}

	//! Creates a connection
	/*
	This operation is synchronous.
	*/
	std::shared_ptr<TCPSocket> connect(const char* ip, int port, TCPError& ec)
	{
		TCPASSERT(!m_stopped);

		SOCKADDR_IN addr;
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &(addr.sin_addr));

		auto sock = std::make_shared<TCPSocket>();
		sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock->m_s == INVALID_SOCKET)
		{
			ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return nullptr;
		}

		TCPINFO("Connect socket=%d", (int)sock->m_s);

		if (::connect(sock->m_s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return nullptr;
		}

		details::utils::setBlocking(sock->m_s, false);

		sock->m_owner = this;
		sock->m_localAddr = details::utils::getLocalAddr(sock->m_s);
		sock->m_peerAddr = details::utils::getRemoteAddr(sock->m_s);
		TCPINFO("Socket %d connected to %s:%d", (int)sock->m_s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second);

		ec = TCPError();
		return sock;
	}

	/*! Asynchronously creates a connection
	*/
	void connect(const char* ip, int port, ConnectHandler h, int timeoutMs = 200)
	{
		TCPASSERT(!m_stopped);

		SOCKADDR_IN addr;
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &(addr.sin_addr));

		auto sock = std::make_shared<TCPSocket>();
		sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		// If the socket creation fails, send the handler to the service thread to execute with an error
		if (sock->m_s == INVALID_SOCKET)
		{
			TCPError ec = details::ErrorWrapper().getError();
			addCmd([ec = std::move(ec), h = std::move(h)]
			{
				h(ec, nullptr);
			});
		}
		TCPINFO("Connect socket=%d", (int)sock->m_s);

		// Set to non-blocking, so we can do an asynchronous connect
		details::utils::setBlocking(sock->m_s, false);
		if (::connect(sock->m_s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			details::ErrorWrapper err;
			if (err.isBlockError())
			{
				// Normal behavior, so setup the connect detection with select
				addCmd([this, sock = std::move(sock), h = std::move(h), timeoutMs]
				{
					ConnectOp op;
					op.timeoutPoint = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(timeoutMs);
					op.h = std::move(h);
					m_connects[sock] = std::move(op);
				});
			}
			else
			{
				// Anything else than a blocking error is a real error
				addCmd([ec = err.getError(), h = std::move(h)]
				{
					h(ec, nullptr);
				});
			}
		}
		else
		{
			// It may happen that the connect succeeds right away.
			addCmd([sock = std::move(sock), h = std::move(h)]
			{
				h(TCPError(), std::move(sock));
			});
		}
	}

	//! Processes whatever it needs
	// \return
	//		false : We are shutting down, and no need to call again
	//		true  : We should call tick again
	bool tick()
	{
		//
		// Execute any pending commands
		m_cmdQueue([&](CmdQueue& q)
		{
			std::swap(q, m_tmpQueue);
		});
		bool finished = false;
		while (!finished && m_tmpQueue.size())
		{
			auto&& fn = m_tmpQueue.front();
			if (fn)
			{
				fn();
			}
			else
			{
				finished = true;
			}
			m_tmpQueue.pop();
		}

		if (finished)
		{
			// If we are finished, then there can't be any commands left
			TCPASSERT(m_tmpQueue.size() == 0);

			//
			// Cancel all handlers in all the sockets we have at the moment
			//
			for (auto&& s : m_connects)
				s.second.h(TCPError(TCPError::Code::Cancelled), nullptr);
			m_connects.clear();

			auto cancel = [](auto&& container)
			{
				for (auto&& s : container)
					s->doCancel();
				container.clear();
			};
			cancel(m_accepts);
			cancel(m_recvs);
			cancel(m_sends);

			return false;
		}

		fd_set readfds, writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		SOCKET maxfd = 0;
		auto addSockets = [&maxfd](auto&& container, fd_set& fds)
		{
			for (auto&& s : container)
			{
				if (s->m_s > maxfd)
					maxfd = s->m_s;
				FD_SET(s->m_s, &fds);
			}
		};
		addSockets(m_accepts, readfds);
		addSockets(m_recvs, readfds);
		addSockets(m_sends, writefds);

		// For non-blocking connects, select will let us know a connect finished through the write fds
		TIMEVAL timeout{ 0,0 };
		if (m_connects.size())
		{
			auto start = std::chrono::high_resolution_clock::now();
			std::chrono::nanoseconds t(std::numeric_limits<long long>::max());
			for (auto&& s : m_connects)
			{
				if (s.first->m_s > maxfd)
					maxfd = s.first->m_s;
				FD_SET(s.first->m_s, &writefds);
				// Calculate timeout
				std::chrono::nanoseconds remain = s.second.timeoutPoint - start;
				if (remain < t)
					t = remain;
			}

			if (t.count() > 0)
			{
				timeout.tv_sec = static_cast<long>(t.count() / (1000 * 1000 * 1000));
				timeout.tv_usec = static_cast<long>((t.count() % (1000 * 1000 * 1000)) / 1000);
			}
		}

		if (readfds.fd_count == 0 && writefds.fd_count == 0)
			return true;

		auto res = select(
			(int)maxfd + 1,
			&readfds,
			&writefds,
			NULL,
			(m_connects.size()) ? &timeout : NULL);

		// get current time, if we are running 
		std::chrono::time_point<std::chrono::high_resolution_clock> end;
		if (m_connects.size())
			end = std::chrono::high_resolution_clock::now();

		if (res == SOCKET_ERROR)
			TCPERROR(details::ErrorWrapper().msg().c_str());

		if (readfds.fd_count)
		{
			for (auto it = m_accepts.begin(); it != m_accepts.end(); )
			{
				if (FD_ISSET((*it)->m_s, &readfds))
				{
					if ((*it)->doAccept())
						++it;
					else
						it = m_accepts.erase(it);
				}
				else
					++it;
			}

			for (auto it = m_recvs.begin(); it != m_recvs.end(); )
			{
				if (FD_ISSET((*it)->m_s, &readfds))
				{
					if ((*it)->doRecv())
						++it;
					else
						it = m_recvs.erase(it);
				}
				else
					++it;
			}
		}

		if (writefds.fd_count)
		{
			// Check writes
			for (auto it = m_sends.begin(); it != m_sends.end(); )
			{
				if (FD_ISSET((*it)->m_s, &writefds))
				{
					if ((*it)->doSend())
						++it;
					else
						it = m_sends.erase(it);
				}
				else
					++it;
			}

			// Check the pending connects
			for (auto it = m_connects.begin(); it != m_connects.end();)
			{
				if (FD_ISSET(it->first->m_s, &writefds))
				{
					auto sock = it->first;
					sock->m_owner = this;
					// #TODO : Test what happens if the getRemoteAddr fails
					sock->m_peerAddr = details::utils::getRemoteAddr(sock->m_s);
					sock->m_localAddr = details::utils::getLocalAddr(sock->m_s);
					TCPINFO("Socket %d connected to %s:%d", (int)sock->m_s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second);
					it->second.h(TCPError(), sock);
					it = m_connects.erase(it);
				}
				else
					++it;
			}

		}

		// Check for expired connection attempts
		for (auto it = m_connects.begin(); it != m_connects.end();)
		{
			if (it->second.timeoutPoint < end)
			{
				it->second.h(TCPError::Code::Timeout, nullptr);
				it = m_connects.erase(it);
			}
			else
			{
				++it;
			}
		}

		return true;
	}


	//! Interrupts any tick calls in progress, and marks service as finishing
	// You should not make any other calls to the service after this
	void stop()
	{
		m_stopped = true;
		// Signal the end by sending an empty command
		addCmd(nullptr);
	}

	//! Used only by the unit tests
protected:
	
	void startSignalIn()
	{
		TCPSocket::RecvOp op;
		op.buf = m_signalInBuf;
		op.bufLen = sizeof(m_signalInBuf);
		op.fill = true;
		op.h = [this](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
				return;
			startSignalIn();
		};
		static_cast<TCPSocket*>(m_signalIn.get())->m_recvs.push(std::move(op));
		m_recvs.insert(std::dynamic_pointer_cast<TCPSocket>(m_signalIn));
	}

};

} // namespace rpc
} // namespace cz
