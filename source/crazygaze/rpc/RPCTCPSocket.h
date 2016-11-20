//
// Excellent BSD socket tutorial:
// http://beej.us/guide/bgnet/
//
// Compatibility stuff (Windows vs Unix)
// https://tangentsoft.net/wskfaq/articles/bsd-compatibility.html
//
// Nice question/answer about socket states, including a neat state diagram:
//	http://stackoverflow.com/questions/5328155/preventing-fin-wait2-when-closing-socket
// Some differences between Windows and Linux:
// https://www.apriorit.com/dev-blog/221-crossplatform-linux-windows-sockets
//
// Windows loopback fast path:
// https://blogs.technet.microsoft.com/wincat/2012/12/05/fast-tcp-loopback-performance-and-low-latency-with-windows-server-2012-tcp-loopback-fast-path/

#pragma once

#ifdef _WIN32
	#define CZRPC_WINSOCK 1
	#include <WinSock2.h>
	#include <WS2tcpip.h>
	#include <strsafe.h>
	#include <mstcpip.h>
	#pragma comment(lib, "Ws2_32.lib")

#ifdef __MINGW32__
	// Bits and pieces missing in MingGW
    #ifndef SIO_LOOPBACK_FAST_PATH
        #define SIO_LOOPBACK_FAST_PATH              _WSAIOW(IOC_VENDOR,16)
    #endif
#endif

#elif __linux__
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/ip.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <fcntl.h>
#endif

#include <set>
#include <stdio.h>
#include <cstdarg>

// Windows defines a min/max macro, interferes with STL
#ifdef max
	#undef max
	#undef min
#endif

namespace cz
{
namespace rpc
{

struct TCPSocketDefaultLog
{
	static void out(bool fatal, const char* type, const char* fmt, ...)
	{
		char buf[256];
		copyStrToFixedBuffer(buf, type);
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1, fmt, args);
		va_end(args);
		printf("%s\n",buf);
		if (fatal)
		{
			CZRPC_DEBUG_BREAK();
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
		ConnectFailed,
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
			case Code::ConnectFailed: return "ConnectFailed";
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
using ConnectHandler = std::function<void(const TCPError&)>;
using TransferHandler = std::function<void(const TCPError& ec, int bytesTransfered)>;

#if _WIN32
	using SocketHandle = SOCKET;
	#define CZRPC_INVALID_SOCKET INVALID_SOCKET
	#define CZRPC_SOCKET_ERROR SOCKET_ERROR
#else
	using SocketHandle = int;
	#define CZRPC_INVALID_SOCKET -1
	#define CZRPC_SOCKET_ERROR -1
#endif


namespace details
{
	// Checks if a specified "Func" type is callable and with the specified signature
	template <typename, typename, typename = void>
	struct check_signature : std::false_type {};

    template <typename Func, typename Ret, typename... Args>
    struct check_signature<
        Func, Ret(Args...),
        typename std::enable_if_t<
            std::is_convertible<decltype(std::declval<Func>()(std::declval<Args>()...)), Ret>::value, void>>
        : std::true_type
    {
    };

	template<typename H>
	using IsTransferHandler = std::enable_if_t<check_signature<H, void(const TCPError&, int)>::value>;
	template<typename H>
	using IsSimpleHandler = std::enable_if_t<check_signature<H, void()>::value>;

	template<typename F>
	struct ScopeGuard
	{
		ScopeGuard(F f) : m_f(std::move(f)) {}
		~ScopeGuard() { m_f(); }
		F m_f;
	};
	template<typename F>
	ScopeGuard<F> scopeGuard(F f)
	{
		return ScopeGuard<F>(std::move(f));
	}

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
		int getCode() const { return err; };
#else
		ErrorWrapper() { err = errno; }
		bool isBlockError() const { return err == EAGAIN || err == EWOULDBLOCK || err == EINPROGRESS; }
		// #TODO Build custom error depending on the error number
		std::string msg() const { return strerror(err); }
		int getCode() const { return err; };
#endif

		TCPError getError() const { return TCPError(TCPError::Code::Other, msg()); }
	private:
		int err;
	};

	struct utils
	{
		// Adapted from http://stackoverflow.com/questions/1543466/how-do-i-change-a-tcp-socket-to-be-non-blocking
		static void setBlocking(SocketHandle s, bool blocking)
		{
			TCPASSERT(s != CZRPC_INVALID_SOCKET);
#if _WIN32
			// 0: Blocking. !=0 : Non-blocking
			u_long mode = blocking ? 0 : 1;
			int res = ioctlsocket(s, FIONBIO, &mode);
			if (res != 0)
				TCPFATAL(ErrorWrapper().msg().c_str());
#else
			int flags = fcntl(s, F_GETFL, 0);
			if (flags <0)
				TCPFATAL(ErrorWrapper().msg().c_str());
			flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
			if (fcntl(s, F_SETFL, flags) != 0)
				TCPFATAL(ErrorWrapper().msg().c_str());
#endif
		}

		static void optimizeLoopback(SocketHandle s)
		{
#if _WIN32
			int optval = 1;
			DWORD NumberOfBytesReturned = 0;
			int status =
				WSAIoctl(
					s,
					SIO_LOOPBACK_FAST_PATH,
					&optval,
					sizeof(optval),
					NULL,
					0,
					&NumberOfBytesReturned,
					0,
					0);

			if (status==CZRPC_SOCKET_ERROR)
			{
				ErrorWrapper err;
				if (err.getCode() == WSAEOPNOTSUPP)
				{
					// This system is not Windows Windows Server 2012, and the call is not supported.
					// Do nothing
				}
				else 
				{
					TCPFATAL(err.msg().c_str());
				}
			}
#endif
		}

		static void closeSocket(SocketHandle s)
		{
#if _WIN32
			::shutdown(s, SD_BOTH);
			::closesocket(s);
#else
			::shutdown(s, SHUT_RDWR);
			::close(s);
#endif
		}

		static void disableNagle(SocketHandle s)
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

		static void setReuseAddress(SocketHandle s)
		{
			int optval = 1;
			int res = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
			if (res != 0)
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

		static std::pair<std::string, int> getLocalAddr(SocketHandle s)
		{
			sockaddr_in addr;
			socklen_t size = sizeof(addr);
			if (getsockname(s, (sockaddr*)&addr, &size) != CZRPC_SOCKET_ERROR && size == sizeof(addr))
				return addrToPair(addr);
			else
			{
				TCPFATAL(ErrorWrapper().msg().c_str());
				return std::make_pair("", 0);
			}
		}

		static std::pair<std::string, int> getRemoteAddr(SocketHandle s)
		{
			sockaddr_in addr;
			socklen_t size = sizeof(addr);
			if (getpeername(s, (sockaddr*)&addr, &size) != CZRPC_SOCKET_ERROR)
				return addrToPair(addr);
			else
			{
				TCPFATAL(ErrorWrapper().msg().c_str());
				return std::make_pair("", 0);
			}
		}
	};

#if _WIN32
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
#endif

	template<typename T>
	class Callstack
	{
	public:
		class Context
		{
		public:
			Context(T* val)
				: m_val(val)
				, m_next(Callstack<T>::ms_top)
			{
				Callstack<T>::ms_top = this;
			}
			~Context()
			{
				Callstack<T>::ms_top = m_next;
			}
			T* getValue()
			{
				return m_val;
			}
		private:
			friend Callstack;
			T* m_val;
			Context* m_next;
		};

		static bool contains(T* val)
		{
			auto p = ms_top;
			while (p)
			{
				if (p->getValue() == val)
					return true;
				p = p->m_next;
			}
			return false;
		}
	private:
		static thread_local Context* ms_top;
	};
	template <typename T>
	thread_local typename Callstack<T>::Context* Callstack<T>::ms_top = nullptr;

	class TCPServiceData;
	//////////////////////////////////////////////////////////////////////////
	// TCPBaseSocket
	//////////////////////////////////////////////////////////////////////////
	class TCPBaseSocket
	{
	public:
		TCPBaseSocket() {}
		virtual ~TCPBaseSocket()
		{
			//releaseHandle();
		}

		bool isValid() const
		{
			return m_s != CZRPC_INVALID_SOCKET;
		}

	protected:

		TCPBaseSocket(const TCPBaseSocket&) = delete;
		void operator=(const TCPBaseSocket&) = delete;

		/*
		void releaseHandle()
		{
			details::utils::closeSocket(m_s);
			m_s = CZRPC_INVALID_SOCKET;
		}
		*/

		friend TCPServiceData;
		friend TCPService;
		SocketHandle m_s = CZRPC_INVALID_SOCKET;
	};

	// This is not part of the TCPService class, so we can break the circular dependency
	// between TCPAcceptor/TCPSocket and TCPService
	class TCPServiceData
	{
	public:
		TCPServiceData()
			: m_stopped(false)
			, m_signalFlight(0)
		{
		}

		bool tickingInThisThread()
		{
			return Callstack<TCPServiceData>::contains(this);
		}

	protected:

		friend TCPSocket;
		friend TCPAcceptor;

#if _WIN32
		details::WSAInstance m_wsaInstance;
#endif
		std::unique_ptr<TCPBaseSocket> m_signalIn;
		std::unique_ptr<TCPBaseSocket> m_signalOut;

		std::atomic<int> m_signalFlight;
		std::atomic<bool> m_stopped; // A "stop" command was enqueued
		bool m_finishing = false; // The stop command was found, and we are in the process of executing any remaining commands

		struct ConnectOp
		{
			std::chrono::time_point<std::chrono::high_resolution_clock> timeoutPoint;
			ConnectHandler h;
		};

		std::unordered_map<TCPSocket*, ConnectOp> m_connects;  // Pending connects
		std::set<TCPAcceptor*> m_accepts; // pending accepts
		std::set<TCPSocket*> m_recvs; // pending reads
		std::set<TCPSocket*> m_sends; // pending writes

		using CmdQueue = std::queue<std::function<void()>>;
		Monitor<CmdQueue> m_cmdQueue;
		CmdQueue m_tmpQueue;
		char m_signalInBuf[1];
		void signal()
		{
			if (!m_signalOut)
				return;
			if (m_signalFlight.load() > 0)
				return;
			char buf = 0;
			++m_signalFlight;
			if (::send(m_signalOut->m_s, &buf, 1, 0) != 1)
				TCPFATAL(details::ErrorWrapper().msg().c_str());
		}

		template< typename H, typename = IsSimpleHandler<H> >
		void addCmd(H&& h)
		{
			m_cmdQueue([&](CmdQueue& q)
			{
				q.push(std::move(h));
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

	TCPSocket(details::TCPServiceData& serviceData)
		: m_owner(serviceData)
	{
	}

	virtual ~TCPSocket()
	{
		TCPASSERT(m_recvs.size() == 0);
		TCPASSERT(m_sends.size() == 0);
		releaseHandle();
	}

	// #TODO : Remove or make private/protected
	// Move to TCPBaseSocket
	void releaseHandle()
	{
		//printf("%p : releaseHandle()\n", this);
		TCPASSERT(m_recvs.size() == 0);
		TCPASSERT(m_sends.size() == 0);
		TCPASSERT(m_owner.m_sends.find(this) == m_owner.m_sends.end());
		TCPASSERT(m_owner.m_recvs.find(this) == m_owner.m_recvs.end());
		details::utils::closeSocket(m_s);
		m_s = CZRPC_INVALID_SOCKET;
	}

	TCPError connect(const char* ip, int port)
	{
		TCPASSERT(m_s == CZRPC_INVALID_SOCKET);
		TCPASSERT(!m_owner.m_stopped);

		m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_s == CZRPC_INVALID_SOCKET)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return ec;
		}

		TCPINFO("Connect socket=%d", (int)m_s);
		// Enable any loopback optimizations (in case this socket is used in loopback)
		details::utils::optimizeLoopback(m_s);

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &(addr.sin_addr));
		if (::connect(m_s, (const sockaddr*)&addr, sizeof(addr)) == CZRPC_SOCKET_ERROR)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			releaseHandle();
			return ec;
		}

		details::utils::setBlocking(m_s, false);

		m_localAddr = details::utils::getLocalAddr(m_s);
		m_peerAddr = details::utils::getRemoteAddr(m_s);
		TCPINFO("Socket %d connected to %s:%d", (int)m_s, m_peerAddr.first.c_str(), m_peerAddr.second);

		return TCPError();
	}

	void asyncConnect(const char* ip, int port, ConnectHandler h, int timeoutMs = 200)
	{
		TCPASSERT(m_s == CZRPC_INVALID_SOCKET);
		TCPASSERT(!m_owner.m_stopped);

		m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		// If the socket creation fails, send the handler to the service thread to execute with an error
		if (m_s == CZRPC_INVALID_SOCKET)
		{
			TCPError ec = details::ErrorWrapper().getError();
			m_owner.addCmd([ec = std::move(ec), h = std::move(h)]
			{
				h(ec);
			});
			return;
		}
		TCPINFO("Connect socket=%d", (int)m_s);

		// Enable any loopback optimizations (in case this socket is used in loopback)
		details::utils::optimizeLoopback(m_s);
		// Set to non-blocking, so we can do an asynchronous connect
		details::utils::setBlocking(m_s, false);

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &(addr.sin_addr));

		if (::connect(m_s, (const sockaddr*)&addr, sizeof(addr)) == CZRPC_SOCKET_ERROR)
		{
			details::ErrorWrapper err;
			if (err.isBlockError())
			{
				// Normal behavior, so setup the connect detection with select
				m_owner.addCmd([this, h = std::move(h), timeoutMs]
				{
					details::TCPServiceData::ConnectOp op;
					op.timeoutPoint = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(timeoutMs);
					op.h = std::move(h);
					m_owner.m_connects[this] = std::move(op);
				});
			}
			else
			{
				// Anything else than a blocking error is a real error
				m_owner.addCmd([this, ec = err.getError(), h = std::move(h)]
				{
					releaseHandle();
					h(ec);
				});
			}
		}
		else
		{
			// It may happen that the connect succeeds right away.
			m_owner.addCmd([h = std::move(h)]
			{
				h(TCPError());
			});
		}
	}

	//
	// Asynchronous reading
	//
	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncReadSome(char* buf, int len, H&& h)
	{
		asyncReadImpl(buf, len, std::forward<H>(h), false);
	}
	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncReadSome(TCPBuffer& buf, H&& h)
	{
		asyncReadImpl(buf.buf.get(), buf.size, std::forward<H>(h), false);
	}
	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncRead(char* buf, int len, H&& h)
	{
		asyncReadImpl(buf, len, std::forward<H>(h), true);
	}
	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncRead(TCPBuffer& buf, H&& h)
	{
		asyncReadImpl(buf.buf.get(), buf.size, std::forward<H>(h), true);
	}

	//
	// Asynchronous sending
	//
	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncWrite(const char* buf, int len, H&& h)
	{
		SendOp op;
		op.buf = buf;
		op.bufLen = len;
		op.h = std::move(h);
		m_owner.addCmd([this, op = std::move(op)]
		{
			if (!isValid())
			{
				op.h(TCPError(TCPError::Code::Other, "asyncWrite called on a closed socket"), 0);
				return;
			}
			m_sends.push(std::move(op));
			m_owner.m_sends.insert(this);
		});
	}

	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncWrite(const TCPBuffer& buf, H&& h)
	{
		asyncWrite(buf.buf.get(), buf.size, std::forward<H>(h));
	}

	//! Cancels all outstanding asynchronous operations
	template< typename H, typename = details::IsSimpleHandler<H> >
	void asyncCancel(H&& h)
	{
		m_owner.addCmd([this, h=std::move(h)]
		{
			doCancel();
			m_owner.m_recvs.erase(this);
			m_owner.m_sends.erase(this);
			h();
		});
	}

	template< typename H, typename = details::IsSimpleHandler<H> >
	void asyncClose(H&& h)
	{
		asyncCancel([this, h=std::move(h)]()
		{
			releaseHandle();
			h();
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

protected:

	template< typename H, typename = details::IsTransferHandler<H> >
	void asyncReadImpl(char* buf, int len, H&& h, bool fill)
	{
		RecvOp op;
		op.buf = buf;
		op.bufLen = len;
		op.fill = fill;
		op.h = std::move(h);
		m_owner.addCmd([this, op = std::move(op)]
		{
			if (!isValid())
			{
				op.h(TCPError(TCPError::Code::Other, "asyncRead/asyncReadSome called on a closed socket"), 0);
				return;
			}

			m_recvs.push(std::move(op));
			m_owner.m_recvs.insert(this);
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
		TransferHandler h;
	};
	struct SendOp
	{
		const char* buf = nullptr;
		int bufLen = 0;
		int bytesTransfered = 0;
		TransferHandler h;
	};

	friend TCPService;
	friend TCPAcceptor;
	details::TCPServiceData& m_owner;
	std::pair<std::string, int> m_localAddr;
	std::pair<std::string, int> m_peerAddr;

	bool doRecv()
	{
		// NOTE:
		// The Operation is moved to a local variable and pop before calling the handler, otherwise the TCPSocket
		// destructor can assert as the result of pop itself since the container is not empty.

		TCPASSERT(m_recvs.size());
		while (m_recvs.size())
		{
			RecvOp& op = m_recvs.front();
			int len = ::recv(m_s, op.buf + op.bytesTransfered, op.bufLen - op.bytesTransfered, 0);
			if (len == CZRPC_SOCKET_ERROR)
			{
				details::ErrorWrapper err;
				if (err.isBlockError())
				{
					if (op.bytesTransfered && !op.fill)
					{
						// If this operation doesn't require a full buffer, we call the handler with
						// whatever data we received, and discard the operation
						m_owner.addCmd([op = std::move(op)]
						{
							op.h(TCPError(), op.bytesTransfered);
						});
						m_recvs.pop();
					}

					// Done receiving, since the socket doesn't have more incoming data
					break;
				}
				else
				{
					TCPERROR(err.msg().c_str());
					m_owner.addCmd([op=std::move(op), err]
					{
						op.h(TCPError(TCPError::Code::ConnectionClosed, err.msg()), op.bytesTransfered);
					});
					m_recvs.pop();
				}
			}
			else if (len > 0)
			{
				op.bytesTransfered += len;
				if (op.bufLen == op.bytesTransfered)
				{
					m_owner.addCmd([op=std::move(op)]
					{
						op.h(TCPError(), op.bytesTransfered);
					});
					m_recvs.pop();
				}
			}
			else if (len == 0)
			{
				// Move to a local variable and pop before calling, otherwise TCPSocket destructor
				// can assert as the result of popping itself since the container is not empty.
				m_owner.addCmd([op=std::move(op)]
				{
					op.h(TCPError(TCPError::Code::ConnectionClosed), op.bytesTransfered);
				});
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
		// NOTE:
		// The Operation is moved to a local variable and pop before calling the handler, otherwise the TCPSocket
		// destructor can assert as the result of pop itself since the container is not empty.

		while (m_sends.size())
		{
			SendOp& op = m_sends.front();
			auto res = ::send(m_s, op.buf + op.bytesTransfered, op.bufLen - op.bytesTransfered, 0);
			if (res == CZRPC_SOCKET_ERROR)
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
					m_owner.addCmd([op=std::move(op), err]
					{
						op.h(TCPError(TCPError::Code::ConnectionClosed, err.msg()), op.bytesTransfered);
					});
					m_sends.pop();
				}
			}
			else
			{
				op.bytesTransfered += res;
				if (op.bufLen == op.bytesTransfered)
				{
					m_owner.addCmd([op=std::move(op)]
					{
						op.h(TCPError(), op.bytesTransfered);
					});
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
			m_owner.addCmd([op=std::move(m_recvs.front())]
			{
				op.h(TCPError::Code::Cancelled, op.bytesTransfered);
			});
			m_recvs.pop();
		}

		while (m_sends.size())
		{
			m_owner.addCmd([op=std::move(m_sends.front())]
			{
				op.h(TCPError::Code::Cancelled, op.bytesTransfered);
			});
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

	TCPAcceptor(details::TCPServiceData& serviceData)
		: m_owner(serviceData)
	{
	}

	virtual ~TCPAcceptor()
	{
		TCPASSERT(m_accepts.size() == 0);
		releaseHandle();
	}

	void releaseHandle()
	{
		TCPASSERT(m_accepts.size() == 0);
		TCPASSERT(m_owner.m_accepts.find(this) == m_owner.m_accepts.end());
		details::utils::closeSocket(m_s);
		m_s = CZRPC_INVALID_SOCKET;
	}

	using AcceptHandler = std::function<void(const TCPError& ec)>;
	template<typename H>
	using IsAcceptHandler = std::enable_if_t<details::check_signature<H, void(const TCPError&)>::value>;

	//! Starts listening for new connections at the specified port
	/*
	\param port
		What port to listen on. If 0, the OS will pick a port from the dynamic range
	\param ec
		If an error occurs, this contains the error.
	\param backlog
		Size of the the connection backlog.
		Also, this is only an hint to the OS. It's not guaranteed.
	\return
		The Acceptor socket, or nullptr, if there was an error
	*/
	TCPError listen(int port, int backlog)
	{
		TCPASSERT(!isValid());

		m_s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_s == CZRPC_INVALID_SOCKET)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			return ec;
		}

		details::utils::setReuseAddress(m_s);

		TCPINFO("Listen socket=%d", (int)m_s);
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (::bind(m_s, (const sockaddr*)&addr, sizeof(addr)) == CZRPC_SOCKET_ERROR)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			releaseHandle();
			return ec;
		}

		if (::listen(m_s, backlog) == CZRPC_SOCKET_ERROR)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			releaseHandle();
			return ec;
		}

		// Enable any loopback optimizations (in case this socket is used in loopback)
		details::utils::optimizeLoopback(m_s);

		m_localAddr = details::utils::getLocalAddr(m_s);

		return TCPError();
	}

	//! Starts an asynchronous accept
	template< typename H, typename = IsAcceptHandler<H> >
	void asyncAccept(TCPSocket& sock, H&& h)
	{
		m_owner.addCmd([this, sock=&sock, h(std::move(h))]
		{
			m_accepts.push({*sock, std::move(h)});
			m_owner.m_accepts.insert(this);
		});
	}

	//! Cancels all outstanding asynchronous operations
	template<typename H, typename = details::IsSimpleHandler<H> >
	void asyncCancel(H&& h)
	{
		m_owner.addCmd([this, h=std::move(h)]
		{
			doCancel();
			m_owner.m_accepts.erase(this);
			h();
		});
	}

	//! Disconnects the socket
	template<typename H, typename = details::IsSimpleHandler<H> >
	void asyncClose(H&& h)
	{
		asyncCancel([this, h=std::move(h)]()
		{
			releaseHandle();
			h();
		});
	}

	const std::pair<std::string, int>& getLocalAddress() const
	{
		return m_localAddr;
	}

protected:
	friend class TCPService;

	struct AcceptOp
	{
		TCPSocket& sock;
		AcceptHandler h;
	};

	// Called from TCPSocketSet
	// Returns true if we still have pending accepts, false otherwise
	bool doAccept()
	{
		TCPASSERT(m_accepts.size());

		AcceptOp op = std::move(m_accepts.front());
		m_accepts.pop();

		TCPASSERT(op.sock.m_s == CZRPC_INVALID_SOCKET);
		sockaddr_in clientAddr;
		socklen_t size = sizeof(clientAddr);
		op.sock.m_s = ::accept(m_s, (struct sockaddr*)&clientAddr, &size);
		if (op.sock.m_s == CZRPC_INVALID_SOCKET)
		{
			auto ec = details::ErrorWrapper().getError();
			TCPERROR(ec.msg());
			m_owner.addCmd([op=std::move(op), ec]
			{
				op.h(ec);
			});
			return m_accepts.size() > 0;
		}
		op.sock.m_localAddr = details::utils::getLocalAddr(op.sock.m_s);
		op.sock.m_peerAddr = details::utils::getRemoteAddr(op.sock.m_s);
		details::utils::setBlocking(op.sock.m_s, false);
		TCPINFO("Server side socket %d connected to %s:%d, socket %d",
			(int)op.sock.m_s, op.sock.m_peerAddr.first.c_str(), op.sock.m_peerAddr.second,
			(int)m_s);
		m_owner.addCmd([op=std::move(op)]
		{
			op.h(TCPError());
		});

		return m_accepts.size() > 0;
	}

	void doCancel()
	{
		while (m_accepts.size())
		{
			m_owner.addCmd([op=std::move(m_accepts.front())]
			{
				op.h(TCPError::Code::Cancelled);
			});
			m_accepts.pop();
		}
	}

	details::TCPServiceData& m_owner;
	std::queue<AcceptOp> m_accepts;
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
private:
	struct Callstack
	{
	};

public:
	TCPService()
	{
		TCPAcceptor dummyListen(*this);
		TCPError ec = dummyListen.listen(0, 1);
		TCPASSERT(!ec);
		m_signalIn = std::make_unique<TCPSocket>(*this);
		bool signalInConnected = false;
		dummyListen.asyncAccept(*reinterpret_cast<TCPSocket*>(m_signalIn.get()), [this, &signalInConnected](const TCPError& ec)
		{
			TCPASSERT(!ec);
			signalInConnected = true;
			TCPINFO("m_signalIn socket=%d", (int)(m_signalIn->m_s));
		});

		// Do this temporary ticking in a different thread, since our signal socket
		// is connected here synchronously
		TCPINFO("Dummy listen socket=%d", (int)(dummyListen.m_s));
		auto tmptick = std::async(std::launch::async, [this, &signalInConnected]
		{
			while (!signalInConnected)
				tick();
		});

		m_signalOut = std::make_unique<TCPSocket>(*this);
		ec = reinterpret_cast<TCPSocket*>(m_signalOut.get())->connect("127.0.0.1", dummyListen.m_localAddr.second);
		TCPASSERT(!ec);
		TCPINFO("m_signalOut socket=%d", (int)(m_signalOut->m_s));
		tmptick.get(); // Wait for the std::async to finish

		// Initiate reading for the dummy input socket
		startSignalIn();

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

		// Releasing these here, instead of letting it be cleared up automatically,
		// so that TCPSocket can do asserts that use their owner (TCPService) while all
		// the owner's members are all valid
		m_signalOut = nullptr;
		m_signalIn = nullptr;
	}

	static TCPService& getFrom(TCPAcceptor& s)
	{
		return static_cast<TCPService&>(s.m_owner);
	}
	static TCPService& getFrom(TCPSocket& s)
	{
		return static_cast<TCPService&>(s.m_owner);
	}


	// Gets commands from the command queue into the temporary command queue for processing
	bool prepareTmpQueue()
	{
		// The temporary queue might still have elements, so don't get more items if that's the case
		if (m_tmpQueue.size() == 0)
		{
			m_cmdQueue([&](CmdQueue& q)
			{
				std::swap(q, m_tmpQueue);
			});
		}
		return m_tmpQueue.size() > 0;
	}

	//! Processes whatever it needs
	// \return
	//		false : We are shutting down, and no need to call again
	//		true  : We should call tick again
	bool tick()
	{
		// put a marker on the callstack, so other code can detect when inside the tick function
		typename details::Callstack<details::TCPServiceData>::Context ctx(this);

		// Continue executing commands until the queue is empty
		while (prepareTmpQueue())
		{
			while (m_tmpQueue.size())
			{
				auto&& fn = m_tmpQueue.front();
				// Since we are calling unknown code (the handler), which might throw an exception,
				// we need to make sure we still remove the handler from the queue
				auto guard = details::scopeGuard([this] { m_tmpQueue.pop(); });
				if (fn)
					fn();
				else
					m_finishing = true;
			}

			if (m_finishing)
			{
				// If we are finished, then there can't be any commands left
				TCPASSERT(m_tmpQueue.size() == 0);

				//
				// Cancel all handlers in all the sockets we have at the moment
				//
				for (auto&& s : m_connects)
				{
					addCmd([h=std::move(s.second.h)]
					{
						h(TCPError(TCPError::Code::Cancelled));
					});
				}
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
			}
		}

		if (m_finishing)
			return false;

		if (m_connects.size() == 0 && m_accepts.size() == 0 && m_recvs.size() == 0 && m_sends.size() == 0)
			return true;

		fd_set readfds, writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		SocketHandle maxfd = 0;
		auto addSockets = [&maxfd](auto&& container, fd_set& fds)
		{
			for (auto&& s : container)
			{
				assert(s->m_s != CZRPC_INVALID_SOCKET);
				if (s->m_s > maxfd)
					maxfd = s->m_s;
				FD_SET(s->m_s, &fds);
			}
		};
		addSockets(m_accepts, readfds);
		addSockets(m_recvs, readfds);
		addSockets(m_sends, writefds);

		// For non-blocking connects, select will let us know a connect finished through the write fds
		timeval timeout{ 0,0 };
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

		if (res == CZRPC_SOCKET_ERROR)
			TCPERROR(details::ErrorWrapper().msg().c_str());

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
				// Check if we are really connected, or something else happened.
				// Windows seems to just wait for the timeout, but Linux gets here before the timeout.
				// So, we need to check if connected or if it was an error
				int result;
				socklen_t result_len = sizeof(result);
				if (getsockopt(sock->m_s, SOL_SOCKET, SO_ERROR, (char*)&result, &result_len) == -1)
				{
					TCPFATAL(details::ErrorWrapper().msg().c_str());
				}

				if (result == 0)
				{
					// #TODO : Test what happens if the getRemoteAddr fails
					sock->m_peerAddr = details::utils::getRemoteAddr(sock->m_s);
					sock->m_localAddr = details::utils::getLocalAddr(sock->m_s);
					TCPINFO("Socket %d connected to %s:%d", (int)sock->m_s, sock->m_peerAddr.first.c_str(), sock->m_peerAddr.second);
					addCmd([op=std::move(it->second)]
					{
						op.h(TCPError());
					});
				}
				else
				{
					auto ec = details::ErrorWrapper().getError();
					ec.code = TCPError::Code::ConnectFailed;
					addCmd([sock=it->first, op=std::move(it->second), ec]
					{
						sock->releaseHandle();
						op.h(ec);
					});
				}

				it = m_connects.erase(it);
			}
			else
				++it;
		}

		// Check for expired connection attempts
		for (auto it = m_connects.begin(); it != m_connects.end();)
		{
			if (it->second.timeoutPoint < end)
			{
				addCmd([sock=it->first, op=std::move(it->second)]
				{
					sock->releaseHandle();
					op.h(TCPError::Code::ConnectFailed);
				});
				it = m_connects.erase(it);
			}
			else
			{
				++it;
			}
		}

		return true;
	}

	void run()
	{
		while (tick())
		{
		}
	}


	//! Interrupts any tick calls in progress, and marks the service as finishing
	// You should not make any other calls to the service after this
	void stop()
	{
		m_stopped = true;
		// Signal the end by sending an "empty" command
		// NOTE:
		// Although "nullptr" would be preferable since it would convert to std::function<void()>, addCmd parameter
		// itself is not an std::function for performance reasons. So I need to pass an empty std::function
		addCmd(std::function<void()>());
	}

	template< typename H, typename = details::IsSimpleHandler<H> >
	void post(H&& handler)
	{
		addCmd(std::forward<H>(handler));
	}

	template< typename H, typename = details::IsSimpleHandler<H> >
	void dispatch(H&& handler)
	{
		if (tickingInThisThread())
			handler();
		else
			post(std::forward<H>(handler));
	}

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
			TCPASSERT(bytesTransfered == 1);
			--m_signalFlight;
			auto i = m_signalFlight.load();
			assert(m_signalFlight.load() >= 0);
			startSignalIn();
		};
		static_cast<TCPSocket*>(m_signalIn.get())->m_recvs.push(std::move(op));
		m_recvs.insert(reinterpret_cast<TCPSocket*>(m_signalIn.get()));
	}

};

} // namespace rpc
} // namespace cz
