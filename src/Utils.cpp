/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <Windows.h>
#include <stdio.h>
#include "Utils.h"

void debug_log(const char* format, ...)
{
	char log[512];
	va_list args;
	va_start(args, format);
	vsprintf_s(log, format, args);
	va_end(args);

	OutputDebugStringA(log);
}

void debug_log_mute(const char* , ...)
{ /* muted debug_log */ 
}
