/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    devcon.h

Abstract:

    Device Console header

--*/
#pragma once


#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif // !VC_EXTRALEAN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN



#include <windows.h>
#include <tchar.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <strsafe.h>
#include <io.h>
#include <fcntl.h>

#include "rc_ids.h"



#ifndef ARRAYSIZE
#define ARRAYSIZE(a)                (sizeof(a)/sizeof(a[0]))
#endif

typedef int (*DispatchFunc)(LPCTSTR BaseName, LPCTSTR Machine, DWORD Flags, int argc, LPTSTR argv[]);
typedef int (*CallbackFunc)(HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Index, LPVOID Context);


#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#define INSTANCEID_PREFIX_CHAR TEXT('@') // character used to prefix instance ID's
#define CLASS_PREFIX_CHAR      TEXT('=') // character used to prefix class name
#define WILD_CHAR              TEXT('*') // wild character
#define QUOTE_PREFIX_CHAR      TEXT('\'') // prefix character to ignore wild characters
#define SPLIT_COMMAND_SEP      TEXT(":=") // whole word, indicates end of id's

//
// Devcon.exe command line flags
//
#define DEVCON_FLAG_FORCE       0x00000001

int EnumerateDevices(LPCTSTR hwid, CallbackFunc Callback, LPVOID Context);
__drv_allocatesMem(object) LPTSTR* GetDevMultiSz(HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Prop);
__drv_allocatesMem(object) LPTSTR* GetMultiSzIndexArray(__drv_aliasesMem LPTSTR MultiSz);
void DelMultiSz(__drv_freesMem(object) PZPTSTR Array);


int cmdInstall(LPCTSTR inf, LPCTSTR hwid);
int cmdRemove(LPCTSTR hwid);


//
// UpdateDriverForPlugAndPlayDevices
//
typedef BOOL(WINAPI* UpdateDriverForPlugAndPlayDevicesProto)(HWND hwndParent,
    LPCTSTR HardwareId,
    LPCTSTR FullInfPath,
    DWORD InstallFlags,
    _Out_opt_ PBOOL bRebootRequired
    );


#ifdef _UNICODE
#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesW"
#define SETUPUNINSTALLOEMINF "SetupUninstallOEMInfW"
#else
#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesA"
#define SETUPUNINSTALLOEMINF "SetupUninstallOEMInfA"
#endif
#define SETUPSETNONINTERACTIVEMODE "SetupSetNonInteractiveMode"
#define SETUPVERIFYINFFILE "SetupVerifyInfFile"

//
// exit codes
//
#define EXIT_OK      (0)
#define EXIT_FAIL    (2)


