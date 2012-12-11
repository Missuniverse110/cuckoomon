/*
Cuckoo Sandbox - Automated Malware Analysis
Copyright (C) 2010-2012 Cuckoo Sandbox Developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <windows.h>
#include "hooking.h"
#include "ntapi.h"
#include "log.h"

// only skip Sleep()'s the first five seconds
#define MAX_SLEEP_SKIP_DIFF 5000

static IS_SUCCESS_NTSTATUS();
static const char *module_name = "sleep";

// skipping sleep calls is done while this variable is set to true
static int sleep_skip_active = 1;

// the amount of time skipped, in 100-nanosecond
static LARGE_INTEGER time_skipped;
static LARGE_INTEGER time_start;

HOOKDEF(NTSTATUS, WINAPI, NtDelayExecution,
    __in    BOOLEAN Alertable,
    __in    PLARGE_INTEGER DelayInterval
) {
    NTSTATUS ret = 0;

    // do we want to skip this sleep?
    if(sleep_skip_active != 0) {
        FILETIME ft; LARGE_INTEGER li;
        GetSystemTimeAsFileTime(&ft);
        li.HighPart = ft.dwHighDateTime;
        li.LowPart = ft.dwLowDateTime;

        // check if we're still within the hardcoded limit
        if(li.QuadPart < time_start.QuadPart + MAX_SLEEP_SKIP_DIFF * 10000) {
            time_skipped.QuadPart += -DelayInterval->QuadPart;
            return ret;
        }
        else {
            sleep_skip_active = 0;
        }
    }
    LOQ("l", "Milliseconds", -DelayInterval->QuadPart / 10000);
    return Old_NtDelayExecution(Alertable, DelayInterval);
}

HOOKDEF(void, WINAPI, GetLocalTime,
    __out  LPSYSTEMTIME lpSystemTime
) {
    IS_SUCCESS_VOID();

    Old_GetLocalTime(lpSystemTime);

    LARGE_INTEGER li; FILETIME ft;
    SystemTimeToFileTime(lpSystemTime, &ft);
    li.HighPart = ft.dwHighDateTime;
    li.LowPart = ft.dwLowDateTime;
    li.QuadPart += time_skipped.QuadPart;
    ft.dwHighDateTime = li.HighPart;
    ft.dwLowDateTime = li.LowPart;
    FileTimeToSystemTime(&ft, lpSystemTime);
}

HOOKDEF(void, WINAPI, GetSystemTime,
    __out  LPSYSTEMTIME lpSystemTime
) {
    IS_SUCCESS_VOID();

    Old_GetSystemTime(lpSystemTime);

    LARGE_INTEGER li; FILETIME ft;
    SystemTimeToFileTime(lpSystemTime, &ft);
    li.HighPart = ft.dwHighDateTime;
    li.LowPart = ft.dwLowDateTime;
    li.QuadPart += time_skipped.QuadPart;
    ft.dwHighDateTime = li.HighPart;
    ft.dwLowDateTime = li.LowPart;
    FileTimeToSystemTime(&ft, lpSystemTime);
}

void disable_sleep_skip()
{
    sleep_skip_active = 0;
}

void init_sleep_skip()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    time_start.HighPart = ft.dwHighDateTime;
    time_start.LowPart = ft.dwLowDateTime;
}