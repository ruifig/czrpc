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
		char buf[256];
		strcpy(buf, type);
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

#define TCPASSERT(expr) \
	if (!(expr)) TCPFATAL(#expr)

#define BUILDERROR() \
	TCPError ec(TCPError::Code::Other, details::TCPUtils::getWin32ErrorMsg()); \
	TCPERROR(ec.msg());

#define SETERROR() \
	ec = TCPError(TCPError::Code::Other, details::TCPUtils::getWin32ErrorMsg()); \
	TCPERROR(ec.msg());

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
					TCPFATAL(getWin32ErrorMsg().c_str());
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

		static void setBlocking(SOCKET s, bool blocking)
		{
			TCPASSERT(s != INVALID_SOCKET);
			// 0: Blocking. !=0 : Non-blocking
			u_long mode = blocking ? 0 : 1;
			int res = ioctlsocket(s, FIONBIO, &mode);
			if (res != 0)
				TCPFATAL(getWin32ErrorMsg().c_str());
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
			if (result !=0)
				TCPFATAL(getWin32ErrorMsg().c_str());
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
				TCPFATAL(getWin32ErrorMsg().c_str());
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
				TCPFATAL(getWin32ErrorMsg().c_str());
				return std::make_pair("", 0);
			}
		}

		//
		// Multiple producer, multiple consumer thread safe queue
		//
		template<typename T>
		class SharedQueue
		{
		private:
			std::queue<T> m_queue;
			mutable std::mutex m_mtx;
			std::condition_variable m_data_cond;

			SharedQueue& operator=(const SharedQueue&) = delete;
			SharedQueue(const SharedQueue& other) = delete;

		public:
			SharedQueue(){}

			template<typename... Args>
			void emplace(Args&&... args)
			{
				std::lock_guard<std::mutex> lock(m_mtx);
				m_queue.emplace(std::forward<Args>(args)...);
				m_data_cond.notify_one();
			}

			template<typename T>
			void push(T&& item){
				std::lock_guard<std::mutex> lock(m_mtx);
				m_queue.push(std::forward<T>(item));
				m_data_cond.notify_one();
			}

			//! Tries to pop an item from the queue. It does not block waiting for
			// items.
			// \return Returns true if an Items was retrieved
			bool try_and_pop(T& popped_item){
				std::lock_guard<std::mutex> lock(m_mtx);
				if (m_queue.empty()){
					return false;
				}
				popped_item = std::move(m_queue.front());
				m_queue.pop();
				return true;
			}

			//! Retrieves all items into the supplied queue.
			// This should be more efficient than retrieving one item at a time when a
			// thread wants to process as many items as there are currently in the
			// queue. Example:
			// std::queue<Foo> local;
			// if (q.try_and_popAll(local)) {
			//     ... process items in local ...
			// }
			//
			// \return
			//	True if any items were retrieved
			// \note
			//	Any elements in the destination queue will be lost.
			bool try_and_popAll(std::queue<T>& dest)
			{
				std::lock_guard<std::mutex> lock(m_mtx);
				dest = std::move(m_queue);
				return dest.size()!=0;
			}

			// Retrieves an item, blocking if necessary to wait for items.
			void wait_and_pop(T& popped_item){
				std::unique_lock<std::mutex> lock(m_mtx);
				m_data_cond.wait(lock, [this] { return !m_queue.empty();});
				popped_item = std::move(m_queue.front());
				m_queue.pop();
			}

			//! Retrieves an item, blocking if necessary for the specified duration
			// until items arrive.
			//
			// \return
			//	false : Timed out (There were no items)
			//	true  : Item retrieved
			bool wait_and_pop(T& popped_item, int64_t timeoutMs){
				std::unique_lock<std::mutex> lock(m_mtx);
				if (!m_data_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return !m_queue.empty();}))
					return false;

				popped_item = std::move(m_queue.front());
				m_queue.pop();
				return true;
			}

			//! Checks if the queue is empty
			bool empty() const{
				std::lock_guard<std::mutex> lock(m_mtx);
				return m_queue.empty();
			}

			//! Returns how many items there are in the queue
			unsigned size() const{
				std::lock_guard<std::mutex> lock(m_mtx);
				return static_cast<unsigned>(m_queue.size());
			}
		};
	};
} // namespace details


struct TCPError
{
	enum class Code
	{
		Success,
		Cancelled,
		ConnectionClosed,
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

class TCPService;
class TCPSocket;

class TCPBaseSocket : public std::enable_shared_from_this<TCPBaseSocket>
{
public:
	TCPBaseSocket() {}
	virtual ~TCPBaseSocket();
protected:
	friend class TCPSocket;
	TCPService* m_owner = nullptr;
	SOCKET m_s = INVALID_SOCKET;
};

/*!
With TCPAcceptor you can listen for new connections on a specified port.
Thread Safety:
	Distinct objects  : Safe
	Shared objects : Unsafe
*/
class TCPAcceptor : public TCPBaseSocket
{
public:
	virtual ~TCPAcceptor()
	{
		assert(m_w.size() == 0);
	}
	using AcceptHandler = std::function<void(const TCPError& ec, std::shared_ptr<TCPSocket> sock)>;

	//! Starts an asynchronous accept
	void accept(AcceptHandler h);

	std::shared_ptr<TCPAcceptor> shared_from_this()
	{
		return std::static_pointer_cast<TCPAcceptor>(TCPBaseSocket::shared_from_this());
	}
protected:
	friend class TCPService;

	// Called from TCPSocketSet
	// Returns true if we still have pending accepts, false otherwise
	bool doAccept();
	void stop();
	std::queue<AcceptHandler> m_w;
	std::pair<std::string, int> m_localAddr;
};

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

	std::shared_ptr<char> buf;
	int size;
};

/*!
Main socket class, used to send and receive data

Thread Safety:
	Distinct objects  : Safe
	Shared objects : Unsafe
*/
class TCPSocket : public TCPBaseSocket
{
public:
	virtual ~TCPSocket()
	{
		assert(m_recvs.size() == 0);
		assert(m_sends.size() == 0);
	}
	using Handler = std::function<void(const TCPError& ec, int bytesTransfered)>;

	//
	// Asynchronous reading 
	//
	template<typename H>
	void recv(char* buf, int len, H&& h)
	{
		recvImpl(buf, len, std::forward<H>(h), false);
	}
	template<typename H>
	void recv(TCPBuffer& buf, H&& h)
	{
		recvImpl(buf.buf.get(), buf.size, std::forward<H>(h), false);
	}
	template<typename H>
	void recvFull(char* buf, int len, H&& h)
	{
		recvImpl(buf, len, std::forward<H>(h), true);
	}
	template<typename H>
	void recvFull(TCPBuffer& buf, H&& h)
	{
		recvImpl(buf.buf.get(), buf.size, std::forward<H>(h), true);
	}

	//
	// Asynchronous sending
	//
	void send(const char* buf, int len, Handler h);
	template<typename H>
	void send(const TCPBuffer& buf, H&& h)
	{
		send(buf.buf.get(), buf.size, std::forward<H>(h));
	}

	std::shared_ptr<TCPSocket> shared_from_this()
	{
		return std::static_pointer_cast<TCPSocket>(TCPBaseSocket::shared_from_this());
	}
protected:
	void recvImpl(char* buf, int len, Handler h, bool fill);

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

	friend class TCPService;
	friend class TCPAcceptor;
	std::pair<std::string, int> m_localAddr;
	std::pair<std::string, int> m_peerAddr;

	bool doRecv();
	bool doSend();
	void stop();
	std::queue<RecvOp> m_recvs;
	std::queue<SendOp> m_sends;
};

/*
Thread Safety:
	Distinct objects  : Safe
	Shared objects : Unsafe
*/
class TCPService
{
public:
	TCPService();
	~TCPService();

	//! Creates a listening socket at the specified port
	/*
	This is a synchronous operation.
	*/
	std::shared_ptr<TCPAcceptor> listen(int port, TCPError& ec);

	//!
	/*
	Asynchronously creates a listening socket
	*/
	void listen(int port, std::function<void(const TCPError&, std::shared_ptr<TCPAcceptor>)> h);

	//! Creates a connection
	/*
	This operation is synchronous.
	*/
	std::shared_ptr<TCPSocket> connect(const char* ip, int port, TCPError& ec);

	/*! Asynchronously creates a connection
	*/
	void connect(const char* ip, int port, std::function<void(const TCPError&, std::shared_ptr<TCPSocket>)> h);

	//! Processes whatever it needs
	// \return
	//		false : We are shutting down, and no need to call again
	//		true  : We should call tick again
	bool tick();

	//! Interrupts any tick calls in progress, and marks service as finishing
	// You should not make any other calls to the service after this
	void stop();
protected:
	
	friend class TCPAcceptor;
	friend class TCPSocket;

	details::TCPUtils::WSAInstance m_wsaInstance;
	std::shared_ptr<TCPSocket> m_signalIn;
	std::shared_ptr<TCPSocket> m_signalOut;

	//
	// Used to keep track of the asynchronous listen and accept calls
	//
	std::thread m_asyncOpsTh; // Thread used to process asynchronous "listen" and "connect" calls
	details::TCPUtils::SharedQueue<std::function<void()>> m_asyncOps;
	std::atomic<bool> m_stopped = false;
	void runAsyncOpsThread();

	std::set<std::shared_ptr<TCPAcceptor>> m_accepts; // Set pending accepts
	std::set<std::shared_ptr<TCPSocket>> m_recvs; // Set of pending reads
	std::set<std::shared_ptr<TCPSocket>> m_sends; // Set of pending writes

	using CmdQueue = std::queue<std::function<void()>>;
	Monitor<CmdQueue> m_cmdQueue;
	CmdQueue m_tmpQueue;
	char m_signalInBuf[512];
	template<typename F>
	void addCmd(F&& f)
	{
		m_cmdQueue([&](CmdQueue& q)
		{
			q.push(std::move(f));
		});
		signal();
	}

	void signal();
	void startSignalIn();
};

///////////////////////////////////////////////
//	TCPBaseSocket
///////////////////////////////////////////////
TCPBaseSocket::~TCPBaseSocket()
{
	::closesocket(m_s);
}

///////////////////////////////////////////////
//	TCPAcceptor
///////////////////////////////////////////////
void TCPAcceptor::accept(AcceptHandler h)
{
	m_owner->addCmd([this_=shared_from_this(), h(std::move(h))]
	{
		this_->m_w.push(std::move(h));
		this_->m_owner->m_accepts.insert(this_);
	});
}

bool TCPAcceptor::doAccept()
{
	TCPASSERT(m_w.size());

	auto h = std::move(m_w.front());
	m_w.pop();

	sockaddr_in clientAddr;
	int size = sizeof(clientAddr);
	SOCKET s = ::accept(m_s, (struct sockaddr*)&clientAddr, &size);
	if (s == INVALID_SOCKET)
	{
		BUILDERROR();
		h(ec, nullptr);
		return m_w.size() > 0;
	}

	auto sock = std::make_shared<TCPSocket>();
	sock->m_s = s;
	sock->m_localAddr = details::TCPUtils::getLocalAddr(s);
	sock->m_peerAddr = details::TCPUtils::getRemoteAddr(s);
	sock->m_owner = m_owner;
	details::TCPUtils::setBlocking(sock->m_s, false);
	TCPINFO("Server side socket %d connected to %s:%d, socket %d",
			(int)s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second,
			(int)m_s);
	h(TCPError(), sock);

	return m_w.size()>0;
}

void TCPAcceptor::stop()
{
	while(m_w.size())
	{
		m_w.front()(TCPError::Code::Cancelled, nullptr);
		m_w.pop();
	}
}


///////////////////////////////////////////////
//	TCPSocket
///////////////////////////////////////////////
std::shared_ptr<TCPAcceptor> TCPService::listen(int port, TCPError& ec)
{
	auto sock = std::make_shared<TCPAcceptor>();
	sock->m_owner = this;

	sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock->m_s == INVALID_SOCKET)
	{
		SETERROR();
		return nullptr;
	}

	TCPINFO("Listen socket=%d", (int)sock->m_s);

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (::bind(sock->m_s, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		SETERROR();
		return nullptr;
	}

	if (::listen(sock->m_s, SOMAXCONN) == SOCKET_ERROR)
	{
		SETERROR();
		return nullptr;
	}

	sock->m_localAddr = details::TCPUtils::getLocalAddr(sock->m_s);

	ec = TCPError();
	return sock;
}

void TCPService::listen(int port, std::function<void(const TCPError&, std::shared_ptr<TCPAcceptor>)> h)
{
	assert(!m_stopped);

	m_asyncOps.emplace([this, port, h=std::move(h)]() mutable
	{
		if (m_stopped)
		{
			// Queue the handler to be processed as a result of tick calls
			addCmd([this, h=std::move(h)]
			{
				h(TCPError(TCPError::Code::Cancelled), nullptr);
			});
		}
		else
		{
			TCPError ec;
			auto sock = this->listen(port, ec);
			// Queue the handler to be processed as a result of tick calls
			addCmd([ec, sock=std::move(sock), h=std::move(h)]
			{
				h(ec, sock);
			});
		}
	});
}

std::shared_ptr<TCPSocket> TCPService::connect(const char* ip, int port, TCPError& ec)
{
	assert(!m_stopped);

	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &(addr.sin_addr));

	auto sock = std::make_shared<TCPSocket>();
	sock->m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock->m_s == INVALID_SOCKET)
	{
		SETERROR();
		return nullptr;
	}

	TCPINFO("Connect socket=%d", (int)sock->m_s);

	if (::connect(sock->m_s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		SETERROR();
		return nullptr;
	}

	details::TCPUtils::setBlocking(sock->m_s, false);

	sock->m_owner = this;
	sock->m_localAddr = details::TCPUtils::getLocalAddr(sock->m_s);
	sock->m_peerAddr = details::TCPUtils::getRemoteAddr(sock->m_s);
	TCPINFO("Socket %d connected to %s:%d", (int)sock->m_s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second);

	ec = TCPError();
	return sock;
}

void TCPService::connect(const char* ip, int port, std::function<void(const TCPError&, std::shared_ptr<TCPSocket>)> h)
{
	assert(!m_stopped);
	m_asyncOps.emplace([this, ip = std::string(ip), port, h=std::move(h)]() mutable
	{
		if (m_stopped)
		{
			// Queue the handler to be processed as a result of tick calls
			addCmd([this, h=std::move(h)]
			{
				h(TCPError(TCPError::Code::Cancelled), nullptr);
			});
		}
		else
		{
			TCPError ec;
			auto sock = this->connect(ip.c_str(), port, ec);
			// Queue the handler to be processed as a result of tick calls
			addCmd([ec, sock=std::move(sock), h=std::move(h)]
			{
				h(ec, sock);
			});
		}
	});
}

void TCPSocket::recvImpl(char* buf, int len, Handler h, bool fill)
{
	RecvOp op;
	op.buf = buf;
	op.bufLen = len;
	op.fill = fill;
	op.h = std::move(h);
	m_owner->addCmd([this_=shared_from_this(), op=std::move(op)]
	{
		this_->m_recvs.push(std::move(op));
		this_->m_owner->m_recvs.insert(this_);
	});
}

void TCPSocket::send(const char* buf, int len, Handler h)
{
	SendOp op;
	op.buf = buf;
	op.bufLen = len;
	op.h = std::move(h);
	m_owner->addCmd([this_=shared_from_this(), op=std::move(op)]
	{
		this_->m_sends.push(std::move(op));
		this_->m_owner->m_sends.insert(this_);
	});

}

bool TCPSocket::doRecv()
{
	TCPASSERT(m_recvs.size());

	while(m_recvs.size())
	{
		RecvOp& op = m_recvs.front();
		int len = ::recv(m_s, op.buf+op.bytesTransfered, op.bufLen-op.bytesTransfered, 0);
		if (len == SOCKET_ERROR)
		{
			bool wouldblock = false;
#if CZRPC_WINSOCK
			auto err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				wouldblock = true;
#elif CZRPC_BSD
			auto err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
				wouldblock = true;
#else
			#error "Unknown"
#endif
			else
				TCPERROR(details::TCPUtils::getWin32ErrorMsg().c_str()); 
			if (wouldblock)
			{
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
		}
		else if (len>0)
		{
			op.bytesTransfered += len;
			if (op.bufLen==op.bytesTransfered)
			{
				op.h(TCPError(), op.bytesTransfered);
				m_recvs.pop();
			}
		}
		else if (len==0)
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

	return m_recvs.size()>0;
}

bool TCPSocket::doSend()
{
	while(m_sends.size())
	{
		SendOp& op = m_sends.front();

		auto res = ::send(m_s, op.buf + op.bytesTransfered, op.bufLen - op.bytesTransfered, 0);
		if (res==-1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				// We can't send more data at the moment, so we are done trying
				break;
			}
			else
				TCPERROR(details::TCPUtils::getWin32ErrorMsg().c_str());

			// #TODO : Need to detect disconnect here
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

	return m_sends.size()>0;
}

void TCPSocket::stop()
{
	while(m_recvs.size())
	{
		m_recvs.front().h(TCPError::Code::Cancelled, m_recvs.front().bytesTransfered);
		m_recvs.pop();
	}

	while(m_sends.size())
	{
		m_sends.front().h(TCPError::Code::Cancelled, m_sends.front().bytesTransfered);
		m_sends.pop();
	}
}

///////////////////////////////////////////////
//	TCPSocketSet
///////////////////////////////////////////////

TCPService::TCPService()
{
	m_asyncOpsTh = std::thread([this]
	{
		runAsyncOpsThread();
	});

	TCPError ec;
	auto dummyListen = listen(0, ec);
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

	details::TCPUtils::disableNagle(m_signalIn->m_s);
	details::TCPUtils::disableNagle(m_signalOut->m_s);
}

TCPService::~TCPService()
{
	assert(m_stopped);
	assert(m_asyncOps.size() == 0);
	m_cmdQueue([&](CmdQueue& q)
	{
		assert(q.size() == 0);
	});

	if (m_asyncOpsTh.joinable())
		m_asyncOpsTh.join();
}

void TCPService::stop()
{
	m_stopped = true;
	// Add an empty handler to the helper thread that runs asynchronous accepts and listens.
	// This will cause that thread to then send an empty handler to the servicing thread.
	m_asyncOps.emplace(nullptr);
}

void TCPService::runAsyncOpsThread()
{
	while(true)
	{
		std::function<void()> op;
		m_asyncOps.wait_and_pop(op);
		if (op)
			op();
		else
		{
			// Queue an empty command to the servicing thread, to signal the end
			addCmd(nullptr);
			return; // An empty function signals the end
		}
	}
}

void TCPService::startSignalIn()
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
	m_signalIn->m_recvs.push(std::move(op));
	m_recvs.insert(m_signalIn);
}

void TCPService::signal()
{
	if (!m_signalOut)
		return;
	char buf = 0;
	if (::send(m_signalOut->m_s, &buf, 1, 0)!=1)
		TCPFATAL(details::TCPUtils::getWin32ErrorMsg().c_str());
}

bool TCPService::tick()
{
	//
	// Execute any pending commands
	m_cmdQueue([&](CmdQueue& q)
	{
		std::swap(q, m_tmpQueue);
	});
	bool finished = false;
	while(!finished && m_tmpQueue.size())
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

		// If we are finishing, then there can't be any commands left
		assert(m_tmpQueue.size() == 0);

		for(auto&& s : m_accepts)
			s->stop();
		m_accepts.clear();

		for (auto&& s : m_recvs)
			s->stop();
		m_recvs.clear();

		for (auto&& s : m_sends)
			s->stop();
		m_sends.clear();

		return false;
	}


	fd_set readfds, writefds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	SOCKET maxfd = 0;
	auto addS = [&maxfd](SOCKET s, fd_set& fds)
	{
		if (s > maxfd)
			maxfd = s;
		FD_SET(s, &fds);
	};

	//# TODO : Remove these
	std::string rstr, wstr;

	for(auto&& s : m_accepts)
	{
		rstr += std::to_string((int)s->m_s) + ", ";
		addS(s->m_s, readfds);
	}

	for(auto&& s : m_recvs)
	{
		rstr += std::to_string((int)s->m_s) + ", ";
		addS(s->m_s, readfds);
	}

	for(auto&& s : m_sends)
	{
		wstr += std::to_string((int)s->m_s) + ", ";
		addS(s->m_s, writefds);
	}

	if (readfds.fd_count == 0 && writefds.fd_count == 0)
		return true;

	TCPINFO("SELECT %d, readCount=%d(%s), writeCount=%d(%s)",
			(int)maxfd + 1, readfds.fd_count, rstr.c_str(), writefds.fd_count, wstr.c_str());
	auto res = select((int)maxfd + 1, &readfds, &writefds, NULL, NULL);
	TCPINFO("    SELECT RES=%d", res);
	if (res == SOCKET_ERROR)
		TCPERROR(details::TCPUtils::getWin32ErrorMsg().c_str());

	if (readfds.fd_count)
	{
		for(auto it = m_accepts.begin(); it!=m_accepts.end(); )
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

		for(auto it = m_recvs.begin(); it!=m_recvs.end(); )
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
		for(auto it = m_sends.begin(); it!=m_sends.end(); )
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
	}

	return true;
}

} // namespace rpc
} // namespace cz
