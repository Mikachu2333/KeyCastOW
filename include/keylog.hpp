// Copyright © 2014 Brook Hong. All Rights Reserved.

#ifndef KEYLOG_H_INCLUDED
#define KEYLOG_H_INCLUDED

#include <windows.h>

extern HHOOK kbdhook;
extern HHOOK moshook;

LRESULT CALLBACK LLKeyboardProc(int nCode, WPARAM wp, LPARAM lp);
LRESULT CALLBACK LLMouseProc(int nCode, WPARAM wp, LPARAM lp);

#endif // KEYLOG_H_INCLUDED
