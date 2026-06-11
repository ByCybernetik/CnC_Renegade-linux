/*
**	MinGW stubs for exception / thread registration (full handler is MSVC-only in Except.cpp).
*/

#if (defined(__GNUC__) && defined(_WIN32)) || defined(RENEGADE_LINUX)

#include "always.h"
#include "Except.h"

unsigned long ExceptionReturnStack = 0;
unsigned long ExceptionReturnAddress = 0;
unsigned long ExceptionReturnFrame = 0;

static void (*AppCallback)(void) = NULL;
static char *(*AppVersionCallback)(void) = NULL;
static bool ExitOnException = false;
static bool TryingToExit = false;
static unsigned long MainThreadID = 0;

int Exception_Handler(int exception_code, EXCEPTION_POINTERS *e_info)
{
	(void)exception_code;
	(void)e_info;
	return 0;
}

int Stack_Walk(unsigned long *, int, CONTEXT *)
{
	return 0;
}

bool Lookup_Symbol(void *, char *, int &)
{
	return false;
}

void Load_Image_Helper(void)
{
}

void Register_Thread_ID(unsigned long thread_id, char *thread_name, bool main)
{
	(void)thread_id;
	(void)thread_name;
	if (main) {
		MainThreadID = thread_id;
	}
}

void Unregister_Thread_ID(unsigned long thread_id, char *thread_name)
{
	(void)thread_id;
	(void)thread_name;
}

void Register_Application_Exception_Callback(void (*app_callback)(void))
{
	AppCallback = app_callback;
}

void Register_Application_Version_Callback(char *(*app_ver_callback)(void))
{
	AppVersionCallback = app_ver_callback;
}

void Set_Exit_On_Exception(bool set)
{
	ExitOnException = set;
}

bool Is_Trying_To_Exit(void)
{
	return TryingToExit;
}

unsigned long Get_Main_Thread_ID(void)
{
	return MainThreadID;
}

#endif
