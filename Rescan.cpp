// Rescan.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>

#pragma comment(lib, "SetupAPI.lib")
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

int main() {
    DEVINST rootDevInst;
    CONFIGRET cr;
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

    // Locate root devnode
    cr = CM_Locate_DevNode(&rootDevInst, NULL, CM_LOCATE_DEVNODE_NORMAL);
    if (cr != CR_SUCCESS) {
        printf("Failed to locate root devnode: 0x%.8x\n",cr);
        return 1;
    }

    EnumerateDevices(rootDevInst, FALSE);

    if (xHciDevInst)
    {
        //printf("\n\Scan child devices (ELAN KB/TP controller) behind USB xHCI Host Controller...");
        unsigned int counter = 0;
        do {
            EnumerateDevices(xHciDevInst, TRUE);
            counter++;
            Sleep(10);
        } while (TRUE);
    }
    return 0;
}