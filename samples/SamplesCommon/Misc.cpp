#include "SamplesCommonPCH.h"
#include "Misc.h"

#ifdef __linux
#include <termios.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stropts.h>

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

#if 0
int my_kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    //ungetc(ch, stdin);
    return 1;
  }

  return 0;
}
#endif

static const int STDIN = 0;
int my_kbhit() {
    static bool initialized = false;

    if (! initialized) {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~(ICANON);
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
	 if (bytesWaiting)
		 my_getch();
    return bytesWaiting;
}

bool try_getline(std::string& str)
{
	termios oldt, newt;
	tcgetattr(STDIN, &oldt);

	// Disable echo
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN, TCSANOW, &newt);

	// check if there are any keys pending
	int bytesWaiting;
	ioctl(STDIN, FIONREAD, &bytesWaiting);

	if (bytesWaiting)
	{
		str = "";
		while(bytesWaiting--)
			str += getchar();

		// enable echo
		oldt.c_lflag &= ~ECHO;
		tcsetattr(STDIN, TCSANOW, &oldt);
		//newt.c_lflag |= ECHO;
		//tcsetattr(STDIN, TCSANOW, &newt);

		std::cout << "COMMAND> " << str;
		std::string tmp;
		std::getline(std::cin, tmp);
		str += tmp;
		printf("*%s\n", str.c_str());
	}

	oldt.c_lflag &= ~ECHO;
	tcsetattr(STDIN, TCSANOW, &oldt);
	return str!="";
}

#if 0
int my_kbhit (void)
{
	//changemode(1);
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET (STDIN_FILENO, &rdfs);

  select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
	//changemode(0);
  return FD_ISSET(STDIN_FILENO, &rdfs);

}
#endif

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


