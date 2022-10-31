/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    devcon.cpp

Abstract:

    Device Console
    command-line interface for managing devices

--*/

#include "devcon.h"



struct IdEntry {
    LPCTSTR String;     // string looking for
    LPCTSTR Wild;       // first wild character if any
    BOOL    InstanceId;
};


__drv_allocatesMem(object)
LPTSTR* GetMultiSzIndexArray(__drv_aliasesMem LPTSTR MultiSz)
/*++

Routine Description:

    Get an index array pointing to the MultiSz passed in

Arguments:

    MultiSz - well formed multi-sz string

Return Value:

    array of strings. last entry+1 of array contains NULL
    returns NULL on failure

--*/
{
    LPTSTR scan;
    LPTSTR* array;
    int elements;

    for (scan = MultiSz, elements = 0; scan[0]; elements++) {
        scan += _tcslen(scan) + 1;
    }
    array = new LPTSTR[elements + 2];
    if (!array) {
        return NULL;
    }
    array[0] = MultiSz;
    array++;
    if (elements) {
        for (scan = MultiSz, elements = 0; scan[0]; elements++) {
            array[elements] = scan;
            scan += _tcslen(scan) + 1;
        }
    }
    array[elements] = NULL;
    return array;
}


void DelMultiSz(__drv_freesMem(object) PZPTSTR Array)
/*++

Routine Description:

    Deletes the string array allocated by GetDevMultiSz/GetRegMultiSz/GetMultiSzIndexArray

Arguments:

    Array - pointer returned by GetMultiSzIndexArray

Return Value:

    None

--*/
{
    if (Array) {
        Array--;
        if (Array[0]) {
            delete[] Array[0];
        }
        delete[] Array;
    }
}

__drv_allocatesMem(object)
LPTSTR* GetDevMultiSz(HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Prop)
/*++

Routine Description:

    Get a multi-sz device property
    and return as an array of strings

Arguments:

    Devs    - HDEVINFO containing DevInfo
    DevInfo - Specific device
    Prop    - SPDRP_HARDWAREID or SPDRP_COMPATIBLEIDS

Return Value:

    array of strings. last entry+1 of array contains NULL
    returns NULL on failure

--*/
{
    LPTSTR buffer;
    DWORD size;
    DWORD reqSize;
    DWORD dataType;
    LPTSTR* array;
    DWORD szChars;

    size = 8192; // initial guess, nothing magic about this
    buffer = new TCHAR[(size / sizeof(TCHAR)) + 2];
    if (!buffer) {
        return NULL;
    }
    while (!SetupDiGetDeviceRegistryProperty(Devs, DevInfo, Prop, &dataType, (LPBYTE)buffer, size, &reqSize)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            goto failed;
        }
        if (dataType != REG_MULTI_SZ) {
            goto failed;
        }
        size = reqSize;
        delete[] buffer;
        buffer = new TCHAR[(size / sizeof(TCHAR)) + 2];
        if (!buffer) {
            goto failed;
        }
    }
    szChars = reqSize / sizeof(TCHAR);
    buffer[szChars] = TEXT('\0');
    buffer[szChars + 1] = TEXT('\0');
    array = GetMultiSzIndexArray(buffer);
    if (array) {
        return array;
    }

failed:
    if (buffer) {
        delete[] buffer;
    }
    return NULL;
}


BOOL WildCardMatch(LPCTSTR Item, const IdEntry& MatchEntry)
/*++

Routine Description:

    Compare a single item against wildcard
    I'm sure there's better ways of implementing this
    Other than a command-line management tools
    it's a bad idea to use wildcards as it implies
    assumptions about the hardware/instance ID
    eg, it might be tempting to enumerate root\* to
    find all root devices, however there is a CfgMgr
    API to query status and determine if a device is
    root enumerated, which doesn't rely on implementation
    details.

Arguments:

    Item - item to find match for eg a\abcd\c
    MatchEntry - eg *\*bc*\*

Return Value:

    TRUE if any match, otherwise FALSE

--*/
{
    LPCTSTR scanItem;
    LPCTSTR wildMark;
    LPCTSTR nextWild;
    size_t matchlen;

    //
    // before attempting anything else
    // try and compare everything up to first wild
    //
    if (!MatchEntry.Wild) {
        return _tcsicmp(Item, MatchEntry.String) ? FALSE : TRUE;
    }
    if (_tcsnicmp(Item, MatchEntry.String, MatchEntry.Wild - MatchEntry.String) != 0) {
        return FALSE;
    }
    wildMark = MatchEntry.Wild;
    scanItem = Item + (MatchEntry.Wild - MatchEntry.String);

    for (; wildMark[0];) {
        //
        // if we get here, we're either at or past a wildcard
        //
        if (wildMark[0] == WILD_CHAR) {
            //
            // so skip wild chars
            //
            wildMark = CharNext(wildMark);
            continue;
        }
        //
        // find next wild-card
        //
        nextWild = _tcschr(wildMark, WILD_CHAR);
        if (nextWild) {
            //
            // substring
            //
            matchlen = nextWild - wildMark;
        }
        else {
            //
            // last portion of match
            //
            size_t scanlen = _tcslen(scanItem);
            matchlen = _tcslen(wildMark);
            if (scanlen < matchlen) {
                return FALSE;
            }
            return _tcsicmp(scanItem + scanlen - matchlen, wildMark) ? FALSE : TRUE;
        }
        if (_istalpha(wildMark[0])) {
            //
            // scan for either lower or uppercase version of first character
            //

            //
            // the code suppresses the warning 28193 for the calls to _totupper
            // and _totlower.  This suppression is done because those functions
            // have a check return annotation on them.  However, they don't return
            // error codes and the check return annotation is really being used
            // to indicate that the return value of the function should be looked
            // at and/or assigned to a variable.  The check return annotation means
            // the return value should always be checked in all code paths.
            // We assign the return values to variables but the while loop does not
            // examine both values in all code paths (e.g. when scanItem[0] == 0,
            // neither u nor l will be examined) and it doesn't need to examine
            // the values in all code paths.
            //
#pragma warning( suppress: 28193)
            TCHAR u = _totupper(wildMark[0]);
#pragma warning( suppress: 28193)
            TCHAR l = _totlower(wildMark[0]);
            while (scanItem[0] && scanItem[0] != u && scanItem[0] != l) {
                scanItem = CharNext(scanItem);
            }
            if (!scanItem[0]) {
                //
                // ran out of string
                //
                return FALSE;
            }
        }
        else {
            //
            // scan for first character (no case)
            //
            scanItem = _tcschr(scanItem, wildMark[0]);
            if (!scanItem) {
                //
                // ran out of string
                //
                return FALSE;
            }
        }
        //
        // try and match the sub-string at wildMark against scanItem
        //
        if (_tcsnicmp(scanItem, wildMark, matchlen) != 0) {
            //
            // nope, try again
            //
            scanItem = CharNext(scanItem);
            continue;
        }
        //
        // substring matched
        //
        scanItem += matchlen;
        wildMark += matchlen;
    }
    return (wildMark[0] ? FALSE : TRUE);
}

BOOL WildCompareHwIds(PZPTSTR Array, const IdEntry& MatchEntry)
/*++

Routine Description:

    Compares all strings in Array against Id
    Use WildCardMatch to do real compare

Arguments:

    Array - pointer returned by GetDevMultiSz
    MatchEntry - string to compare against

Return Value:

    TRUE if any match, otherwise FALSE

--*/
{
    if (Array) {
        while (Array[0]) {
            if (WildCardMatch(Array[0], MatchEntry)) {
                return TRUE;
            }
            Array++;
        }
    }
    return FALSE;
}



int EnumerateDevices(LPCTSTR hwid, CallbackFunc Callback, LPVOID Context)
/*++

Routine Description:

    Generic enumerator for devices that will be passed the following arguments:
    <id> [<id>...]
    =<class> [<id>...]
    where <id> can either be @instance-id, or hardware-id and may contain wildcards
    <class> is a class name

Arguments:

    BaseName - name of executable
    Machine  - name of machine to enumerate
    Flags    - extra enumeration flags (eg DIGCF_PRESENT)
    argc/argv - remaining arguments on command line
    Callback - function to call for each hit
    Context  - data to pass function for each hit

Return Value:

    EXIT_xxxx

--*/
{
    //
    // add all id's to list
    // if there's a class, filter on specified class
    //
    GUID cls;
    DWORD numClass = 0;
    HDEVINFO devs = INVALID_HANDLE_VALUE;
    devs = SetupDiGetClassDevsEx(numClass ? &cls : NULL,
        NULL,
        NULL,
        (numClass ? 0 : DIGCF_ALLCLASSES) | DIGCF_PRESENT,
        NULL,
        NULL,
        NULL);

    if (devs == INVALID_HANDLE_VALUE) {
        goto final;
    }

    SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;
    devInfoListDetail.cbSize = sizeof(devInfoListDetail);
    if (!SetupDiGetDeviceInfoListDetail(devs, &devInfoListDetail)) {
        goto final;
    }

    IdEntry matchEntry[] = { hwid };

    SP_DEVINFO_DATA devInfo;
    devInfo.cbSize = sizeof(devInfo);

    BOOL match = FALSE;
    int failcode = EXIT_FAIL;
    for (DWORD devIndex = 0; SetupDiEnumDeviceInfo(devs, devIndex, &devInfo) && !match; devIndex++) {
        LPTSTR* hwIds = NULL;

        //
        // determine hardware ID's
        // and search for matches
        //
        hwIds = GetDevMultiSz(devs, &devInfo, SPDRP_HARDWAREID);
        if (WildCompareHwIds(hwIds, matchEntry[0])) {
            match = TRUE;
        }

        DelMultiSz(hwIds);

        if (match) {
            int retcode = Callback(devs, &devInfo, devIndex, Context);
            if (retcode) {
                failcode = retcode;
                goto final;
            }
        }
    }

    failcode = EXIT_OK;

    final:
    if (devs != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(devs);
    }

    return failcode;
}



int main(int argc, PWSTR* argv)
{
    LPSTR cmd = GetCommandLine();

    LPCTSTR inf = TEXT("WinVirtualDisplay.inf");
    LPCTSTR hwid = TEXT("root\\WinVirtualDisplay");

    int retval = 0;
    if (GetVersion() > 0x0A00295A) {
        if (strstr(cmd, "/install") != NULL) {
            retval = cmdRemove(hwid);
            retval = cmdInstall(inf, hwid);
        }
        else if (strstr(cmd, "/uninstall") != NULL) {
            retval = cmdRemove(hwid);
        }
    }

    return retval;
}
