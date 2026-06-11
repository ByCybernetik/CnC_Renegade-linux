/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _WIN32_WINNT 0x0400

#include "thread.h"
#include "Except.h"
#include "wwdebug.h"
#if !defined(RENEGADE_LINUX)
#include <process.h>
#include <windows.h>
#else
#include <process.h>
#endif
#if defined(_MSC_VER)
#pragma warning ( push )
#pragma warning ( disable : 4201 )
#endif
#include "systimer.h"
#if defined(_MSC_VER)
#pragma warning ( pop )
#endif


ThreadClass::ThreadClass(const char *thread_name, ExceptionHandlerType exception_handler) : handle(0), running(false), thread_priority(0)
{
	if (thread_name) {
		assert(strlen(thread_name) < sizeof(ThreadName) - 1);
		strcpy(ThreadName, thread_name);
	} else {
		strcpy(ThreadName, "No name");;
	}

	ExceptionHandler = exception_handler;
}

ThreadClass::~ThreadClass()
{
	Stop();
}

void __cdecl ThreadClass::Internal_Thread_Function(void* params)
{
	ThreadClass* tc=reinterpret_cast<ThreadClass*>(params);
	tc->running=true;
	tc->ThreadID = GetCurrentThreadId();

#ifdef _WIN32
	Register_Thread_ID(tc->ThreadID, tc->ThreadName);

	if (tc->ExceptionHandler != NULL) {
#if defined(_MSC_VER)
		__try {
			tc->Thread_Function();
		} __except(tc->ExceptionHandler(GetExceptionCode(), GetExceptionInformation())) {};
#else
		/* MinGW: no MSVC SEH; run thread body without structured exception filter. */
		tc->Thread_Function();
#endif
	} else {
		tc->Thread_Function();
	}

#else //_WIN32
	tc->Thread_Function();
#endif //_WIN32

#ifdef _WIN32
	Unregister_Thread_ID(tc->ThreadID, tc->ThreadName);
#endif // _WIN32
	tc->handle=0;
	tc->ThreadID = 0;
}

void ThreadClass::Execute()
{
	WWASSERT(!handle);	// Only one thread at a time!
#if defined(_UNIX) && !defined(RENEGADE_LINUX)
		// assert(0);
		return;
#else
		handle=_beginthread(&Internal_Thread_Function,0,this);
		SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
		WWDEBUG_SAY(("ThreadClass::Execute: Started thread %s, thread ID is %X\n", ThreadName, handle));
#endif
}

void ThreadClass::Set_Priority(int priority)
{
#if defined(_UNIX) && !defined(RENEGADE_LINUX)
		// assert(0);
		return;
#else
		thread_priority=priority;
		if (handle) SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
#endif
}

void ThreadClass::Stop(unsigned ms)
{
#if defined(_UNIX) && !defined(RENEGADE_LINUX)
		// assert(0);
		return;
#else
		running=false;
		unsigned time=TIMEGETTIME();
		while (handle) {
			if ((TIMEGETTIME()-time)>ms) {
				int res=TerminateThread((HANDLE)handle,0);
				WWASSERT(res);	// Thread still not killed!
				handle=0;
			}
			Sleep(0);
		}
#endif
}

void ThreadClass::Sleep_Ms(unsigned ms)
{
	Sleep(ms);
}

#if !defined(_UNIX) || defined(RENEGADE_LINUX)
static HANDLE test_event = ::CreateEvent (NULL, FALSE, FALSE, "");
#endif

void ThreadClass::Switch_Thread()
{
#if defined(_UNIX) && !defined(RENEGADE_LINUX)
		return;
#else
		//	::SwitchToThread ();
		::WaitForSingleObject (test_event, 1);
		//	Sleep(1);	// Note! Parameter can not be 0 (or the thread switch doesn't occur)
#endif
}

// Return calling thread's unique thread id
unsigned ThreadClass::_Get_Current_Thread_ID()
{
#if defined(_UNIX) && !defined(RENEGADE_LINUX)
		return 0;
#else
		return GetCurrentThreadId();
#endif
}

bool ThreadClass::Is_Running()
{
	return !!handle;
}