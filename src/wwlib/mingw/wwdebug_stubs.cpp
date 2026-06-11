/*
**	Minimal wwdebug implementations for standalone MinGW wwlib builds.
*/

#include "wwdebug.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static PrintFunc MessageHandler = NULL;
static AssertPrintFunc AssertHandler = NULL;
static TriggerFunc TriggerHandler = NULL;
static ProfileFunc ProfileStartHandler = NULL;
static ProfileFunc ProfileStopHandler = NULL;

PrintFunc WWDebug_Install_Message_Handler(PrintFunc func)
{
	PrintFunc old = MessageHandler;
	MessageHandler = func;
	return old;
}

AssertPrintFunc WWDebug_Install_Assert_Handler(AssertPrintFunc func)
{
	AssertPrintFunc old = AssertHandler;
	AssertHandler = func;
	return old;
}

TriggerFunc WWDebug_Install_Trigger_Handler(TriggerFunc func)
{
	TriggerFunc old = TriggerHandler;
	TriggerHandler = func;
	return old;
}

ProfileFunc WWDebug_Install_Profile_Start_Handler(ProfileFunc func)
{
	ProfileFunc old = ProfileStartHandler;
	ProfileStartHandler = func;
	return old;
}

ProfileFunc WWDebug_Install_Profile_Stop_Handler(ProfileFunc func)
{
	ProfileFunc old = ProfileStopHandler;
	ProfileStopHandler = func;
	return old;
}

void Convert_System_Error_To_String(int error_id, char *buffer, int buf_len)
{
	if (buffer && buf_len > 0) {
		snprintf(buffer, (size_t)buf_len, "error %d", error_id);
	}
}

int Get_Last_System_Error()
{
	return 0;
}

void WWDebug_Printf(const char *format, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, format);
	vsnprintf(buffer, sizeof(buffer), format, va);
	va_end(va);
	buffer[sizeof(buffer) - 1] = '\0';
	if (MessageHandler) {
		MessageHandler(WWDEBUG_TYPE_INFORMATION, buffer);
	}
}

void WWDebug_Printf_Warning(const char *format, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, format);
	vsnprintf(buffer, sizeof(buffer), format, va);
	va_end(va);
	buffer[sizeof(buffer) - 1] = '\0';
	if (MessageHandler) {
		MessageHandler(WWDEBUG_TYPE_WARNING, buffer);
	}
}

void WWDebug_Printf_Error(const char *format, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, format);
	vsnprintf(buffer, sizeof(buffer), format, va);
	va_end(va);
	buffer[sizeof(buffer) - 1] = '\0';
	if (MessageHandler) {
		MessageHandler(WWDEBUG_TYPE_ERROR, buffer);
	}
}

#ifdef WWDEBUG
void WWDebug_Assert_Fail(const char *expr, const char *file, int line)
{
	(void)expr;
	(void)file;
	(void)line;
	if (AssertHandler) {
		AssertHandler(expr);
	} else {
		abort();
	}
}

void WWDebug_Assert_Fail_Print(const char *expr, const char *file, int line, const char *string)
{
	(void)file;
	(void)line;
	(void)string;
	if (AssertHandler) {
		AssertHandler(expr);
	} else {
		abort();
	}
}

bool WWDebug_Check_Trigger(int trigger_num)
{
	(void)trigger_num;
	return TriggerHandler ? TriggerHandler(trigger_num) : false;
}

void WWDebug_Profile_Start(const char *title)
{
	if (ProfileStartHandler) {
		ProfileStartHandler(title);
	}
}

void WWDebug_Profile_Stop(const char *title)
{
	if (ProfileStopHandler) {
		ProfileStopHandler(title);
	}
}

void WWDebug_DBWin32_Message_Handler(const char *message)
{
	(void)message;
}
#endif
