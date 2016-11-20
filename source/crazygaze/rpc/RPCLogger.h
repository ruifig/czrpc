/*
Simple asynchronous logging.
Not meant to be production ready. Just useful for development
*/
#pragma once

#include <fstream>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <cstdarg>
#include <assert.h>

namespace cz
{

class Logger
{
private:

	// Thread safe queue that blocks until an item arrives
	template<typename T>
	class Queue
	{
	public:

		void push(T val)
		{
			std::lock_guard<std::mutex> lk(m_mtx);
			m_q.push(std::move(val));
			m_cond.notify_one();
		}

		T pop()
		{
			std::unique_lock<std::mutex> lk(m_mtx);
			// wait for an item...
			m_cond.wait(lk, [this] { return !m_q.empty();});
			// Better to move out of the queue than copying
			T ret = std::move(m_q.front());
			m_q.pop();
			return ret;
		}
	private:
		std::queue<std::string> m_q;
		mutable std::mutex m_mtx;
		std::condition_variable m_cond;
	};
public:
	enum class Verbosity
	{
		Fatal,
		Error,
		Warning,
		Log,
	};

	static const char* verbosityStr(Verbosity v)
	{
		switch (v)
		{
		case Verbosity::Fatal: return "FTL";
		case Verbosity::Error: return "ERR";
		case Verbosity::Warning: return "WRN";
		case Verbosity::Log: return "LOG";
		default:
			assert(0);
			return "";
		};
	}

	Logger(const char* filename)
		: m_filename(filename)
	{
		m_file.open(filename, std::ios::out | std::ios::trunc);
		m_iothread = std::thread([this]
		{
			while (true)
			{
				auto s = m_q.pop();
				if (s.empty()) // empty string signal the thread to finish
					break;
				m_file << s << std::endl;
			}
		});
	}

	~Logger()
	{
		m_q.push(""); // empty string signals the thread to finish
		m_iothread.join();
	}

	void log(const char* file, int line, Verbosity verbosity, const char* fmt, ...)
	{
		char buf[1024];
		// format the log entry string
		va_list args;
		va_start(args, fmt);

		time_t t = time(nullptr);
		struct tm d;
		localtime_s(&d, &t);
		snprintf(buf, sizeof(buf) - 1, "%04d-%02d-%02d %02d:%02d:%02d: %s: ",
			1900+d.tm_year, d.tm_mon+1, d.tm_mday,
			d.tm_hour, d.tm_min, d.tm_sec, verbosityStr(verbosity));
		/*
		time_t timer;
		struct tm* tm_info;
		time(&timer);
		tm_info = localtime(&timer);
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S: ", tm_info);
		*/

		auto l = strlen(buf);
		vsnprintf(buf+l, sizeof(buf) - l - 1, fmt, args);
		va_end(args);
		// queue the entry
		m_q.push(buf);
	}

private:
	std::thread m_iothread;
	std::string m_filename;
	std::ofstream m_file;
	Queue<std::string>  m_q;
};

}
