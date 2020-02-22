#pragma once

#include <string>

#ifdef __linux__
#include <termio.h>
#endif

// Cross platform getch()
// Note:
// It will return as soon as a key is pressed.
// Seems to work fine on a linux terminal (doesn't require to hit Enter), but if running inside Clion, it seems we
// still need to press Enter
int my_getch();


int my_kbhit();


class CommandLineReader
{
public:
	explicit CommandLineReader(std::string prompt = "");
	~CommandLineReader();
    bool tryGet(std::string& cmd);
private:
    std::string m_prompt;
#ifdef __linux__
    termios m_originalt;
#endif
};
