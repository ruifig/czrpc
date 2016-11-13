#include "SamplesCommonPCH.h"
#include "Misc.h"

#ifdef __linux
#include <termios.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

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

void changemode(int dir)
{
  static struct termios oldt, newt;

  if ( dir == 1 )
  {
    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
  }
  else
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
}

int my_kbhit (void)
{
	changemode(1);
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET (STDIN_FILENO, &rdfs);

  select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
	changemode(0);
  return FD_ISSET(STDIN_FILENO, &rdfs);

}
#if 0
// From http://www.flipcode.com/archives/_kbhit_for_Linux.shtml
int my_kbhit() {
	timeval timeout;
	fd_set rdset;

	FD_ZERO(&rdset);
	FD_SET(STDIN, &rdset);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	return select(STDIN + 1, &rdset, NULL, NULL, &timeout);
}
#endif

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


