#pragma once

// Cross platform getch()
// Note:
// It will return as soon as a key is pressed.
// Seems to work fine on a linux terminal (doesn't require to hit Enter), but if running inside Clion, it seems we
// still need to press Enter
char my_getch();

int my_kbhit();
bool try_getline(std::string& str);

