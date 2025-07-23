// Rescan.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>
#include <devpkey.h>
#include <iostream>
#include <tchar.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
DEVINST xHciDevInst = 0;
WCHAR * xHCI_Host_Controller = (WCHAR *) L"ACPI\\QCOM0D08\\3";
// Test
//WCHAR* xHCI_Host_Controller = (WCHAR*)L"URS\\VEN_QCOM&DEV_0C8B&SUBSYS_CRD08380";

void EnumerateDevices(DEVINST devInst, BOOLEAN rescan) {
    CONFIGRET cr;
    DEVINST childDevInst = 0;
    DEVINST siblingDevInst = 0;
    WCHAR deviceID[MAX_DEVICE_ID_LEN];

    // Get first child
    cr = CM_Get_Child(&childDevInst, devInst, 0);
    if (cr != CR_SUCCESS) return;

    // Traverse all children
    do {
        // Get device ID string
        ZeroMemory(&deviceID[0], sizeof(deviceID));
        //Get Device Instance Path: "ACPI\QCOM0D08\3"
        cr = CM_Get_Device_ID(childDevInst, deviceID, MAX_DEVICE_ID_LEN, 0);
        
        //if (rescan) 
        //    printf("CM_Get_Device_ID : %ls\n", deviceID);
        if (cr == CR_SUCCESS) {
            // Filter for USB devices
            // ACPI\\QCOM0D08\\3: KB/TP
            // ACPI\\QCOM0D09\\4: Fingerprint reader

            if ((!rescan)&&(wcsncmp(deviceID, xHCI_Host_Controller, wcslen(xHCI_Host_Controller)) == 0)) {
                //printf("Found USB xHCI Controller!\n");
                xHciDevInst = childDevInst;
                break;
            }
        }

        // Recurse into this child node to find deeper USB devices
        EnumerateDevices(childDevInst, rescan);

        // Get the next sibling
        cr = CM_Get_Sibling(&childDevInst, childDevInst, 0);
    
    } while (cr == CR_SUCCESS);
}

bool ChangeDeviceState(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, bool enable) {
    SP_PROPCHANGE_PARAMS params;
    ZeroMemory(&params, sizeof(params));
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    params.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
    params.Scope = DICS_FLAG_GLOBAL;
    params.HwProfile = 0;

    // Set class install parameters
    if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData, &params.ClassInstallHeader, sizeof(params))) {
        std::cerr << "SetupDiSetClassInstallParams failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    // Apply the property change
    if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, &devInfoData)) {
        std::cerr << "SetupDiCallClassInstaller failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

bool RestartDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData) {
    std::cout << "Disabling device..." << std::endl;
    if (!ChangeDeviceState(hDevInfo, devInfoData, false)) {
        return false;
    }
    Sleep(1000);  // Small delay

    std::cout << "Enabling device..." << std::endl;
    if (!ChangeDeviceState(hDevInfo, devInfoData, true)) {
        return false;
    }

    std::cout << "Device restart completed." << std::endl;
    return true;
}


void EnumerateDeviceInfo() {
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        NULL,
        NULL,
        NULL,
        DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_PROFILE
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "SetupDiGetClassDevs failed\n";
        return;
    }

    SP_DEVINFO_DATA devInfoData;

    ZeroMemory(&devInfoData, sizeof(SP_DEVINFO_DATA));
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        TCHAR name[1024];
        DWORD size = 0;

        // Try Friendly Name first
        if (!SetupDiGetDeviceRegistryProperty(
            hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME,
            NULL, (PBYTE)name, sizeof(name), &size)) {
            // Fall back to Device Description
            if (!SetupDiGetDeviceRegistryProperty(
                hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
                NULL, (PBYTE)name, sizeof(name), &size)) {
                _tcscpy_s(name, _T("Unknown Device"));
            }
        }

        // Check device status for Yellow Bang
        ULONG status = 0, problemCode = 0;
        if (CM_Get_DevNode_Status(&status, &problemCode, devInfoData.DevInst, 0) == CR_SUCCESS) {
            if (status & DN_HAS_PROBLEM) {
                std::wcout << L"Device Name: " << name << std::endl;
                std::wcout << L"    Yellow Bang, Problem Code: " << problemCode << L"" << std::endl;

                // Get Hardware IDs
                TCHAR hwid[1024];
                if (SetupDiGetDeviceRegistryProperty(
                    hDevInfo, &devInfoData, SPDRP_HARDWAREID,
                    NULL, (PBYTE)hwid, sizeof(hwid), NULL)) {
                    std::wcout << L"    Hardware ID: " << hwid << std::endl;
                }

                // Get driver version
                WCHAR driverVersion[256] = { 0 };
                DEVPROPTYPE propType;
                if (SetupDiGetDevicePropertyW(
                    hDevInfo, &devInfoData, &DEVPKEY_Device_DriverVersion,
                    &propType, (PBYTE)driverVersion, sizeof(driverVersion), NULL, 0)) {
                    std::wcout << L"    Driver Version: " << driverVersion << std::endl;
                }

                std::wcout << L"----------------------------------" << std::endl;

                // Attempt for recovery by Disable/Enable device 
                if (RestartDevice(hDevInfo, devInfoData))
                {
                    // Check the status again and see whethere it's successfully recovered or not
                    if (CM_Get_DevNode_Status(&status, &problemCode, devInfoData.DevInst, 0) == CR_SUCCESS) {
                        if (status & DN_HAS_PROBLEM) {
                            std::wcout << L"Device Name: " << name << std::endl;
                            std::wcout << L"    Yellow Bang, Problem Code: " << problemCode << L"" << std::endl;
                        }
                        else
                        {
                            std::wcout << L"Device Name: " << name << std::endl;
                            std::wcout << L"    No more yellow bang, recover successfully! " << L"" << std::endl;
                        }
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
}

BOOL IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

int main() {
    DEVINST rootDevInst;
    CONFIGRET cr;

    if (!IsRunAsAdmin())
    {
        std::wcout << L"Please re-run as administrator" << std::endl;
        return false;
    }

    /*
    // Get device info set for all present USB devices
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_USB,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_PROFILE);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return 1;
    }
    unsigned int counter = 0;
    while (true)
    {
        // Trigger a device tree rescan
        CM_Reenumerate_DevNode(NULL, 0);

        Sleep(1000);
        counter++;
        printf("[%3d] Rescan device tree and sleep for 1 second\n\n", counter);
        if (counter == 50)
        {
            printf("Reach counter limit %d, stop the recan daemon\n", counter);
            break;
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
    */

    if (false)
    {
        // Locate root devnode
        cr = CM_Locate_DevNode(&rootDevInst, NULL, CM_LOCATE_DEVNODE_NORMAL);
        if (cr != CR_SUCCESS) {
            printf("Failed to locate root devnode: 0x%.8x\n", cr);
            return 1;
        }

        EnumerateDevices(rootDevInst, FALSE);

        if (xHciDevInst)
        {
            //printf("\n\Scan child devices (ELAN KB/TP controller) behind USB xHCI Host Controller...");
            do {
                EnumerateDevices(xHciDevInst, TRUE);
                Sleep(10);
            } while (TRUE);
        }
    }

    do {
        // Examine if there is any yellow bang device in Device Manager
        EnumerateDeviceInfo();
        Sleep(1000);
    } while (TRUE);
    return 0;
}