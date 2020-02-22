#include "CommonPCH.h"
#include "Misc.h"

#ifdef __linux
#include <sys/select.h>
#include <unistd.h>
#elif _WIN32
#include <conio.h>
#endif

#ifdef __linux__

static termios term_save()
{
    termios t;
    tcgetattr(STDIN_FILENO, &t);
    return t;
}

static void term_restore(termios& t)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void term_setEcho(bool val)
{
    termios t;
    tcgetattr(STDIN_FILENO, &t);
    if (val)
        t.c_lflag |= ECHO;
    else
        t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void term_setBuffering(bool val)
{
    termios t;
    tcgetattr(STDIN_FILENO, &t);
    if (val)
        t.c_lflag |= ICANON;
    else
        t.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

int my_getch()
{
	return getchar();
}

int my_kbhit()
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
	select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

#elif _WIN32

int my_getch()
{
	return _getch();
}

int my_kbhit()
{
	return _kbhit();
}

#endif



CommandLineReader::CommandLineReader(std::string prompt)
            : m_prompt(prompt)
    {
#ifdef __linux__
	m_originalt = term_save();
	term_setBuffering(false);
	term_setEcho(false);
#elif _WIN32
#endif
	std::cout << "Press ENTER to go into command mode." << std::endl;
}

CommandLineReader::~CommandLineReader()
{
#ifdef __linux__
	term_restore(m_originalt);
#endif
}

bool CommandLineReader::tryGet(std::string& cmd)
{
	bool ok = false;
	while (my_kbhit())
	{
		int c = my_getch();
		if (c == '\n' || c == '\r')
			ok = true;
	}

	if (!ok)
		return false;

#ifdef __linux__
	term_setBuffering(true);
	term_setEcho(true);
#endif
	std::cout << m_prompt;
	cmd = "";
	std::getline(std::cin, cmd);
#ifdef __linux__
	term_setBuffering(false);
	term_setEcho(false);
#endif
	return true;
}
