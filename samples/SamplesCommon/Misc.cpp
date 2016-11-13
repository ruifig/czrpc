#include "SamplesCommonPCH.h"
#include "Misc.h"

#ifdef __linux
#include <termios.h>

static struct termios old, new_;

/* Initialize new terminal i/o settings */
static void initTermios(int echo)
{
	tcgetattr(0, &old); /* grab old terminal i/o settings */
	new_ = old; /* make new settings same as old settings */
	new_.c_lflag &= ~ICANON; /* disable buffered i/o */
	new_.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
	tcsetattr(0, TCSANOW, &new_); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void resetTermios(void)
{
	tcsetattr(0, TCSANOW, &old);
}

/* Read 1 character - echo defines echo mode */
static char getch_(int echo)
{
	char ch;
	initTermios(echo);
	ch = getchar();
	resetTermios();
	return ch;
}

/* Read 1 character without echo */
char my_getch(void)
{
	return getch_(0);
}

#else

char my_getch()
{
	return _getch();
}

int my_kbhit()
{
	return _kbhit();
}

#endif


