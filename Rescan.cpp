// Rescan.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>

#pragma comment(lib, "SetupAPI.lib")
void EnumerateDevices(DEVINST devInst) {
    CONFIGRET cr;
    DEVINST childDevInst = 0;
    DEVINST xHciDevInst = 0;
    DEVINST siblingDevInst = 0;
    WCHAR deviceID[MAX_DEVICE_ID_LEN];

    // Get first child
    cr = CM_Get_Child(&childDevInst, devInst, 0);
    if (cr != CR_SUCCESS) return;

    // Traverse all children
    do {
        // Get device ID string
        cr = CM_Get_Device_ID(childDevInst, deviceID, MAX_DEVICE_ID_LEN, 0);
        //printf("CM_Get_Device_ID : %ls\n",deviceID);
        if (cr == CR_SUCCESS) {
            // Filter for USB devices
            //if (wcsncmp(deviceID, L"ACPI\\VEN_QCOM&DEV_0D08", 22) == 0) {
            if (wcsncmp(deviceID, L"ACPI\\VEN_QCOM&DEV_06A1", 8) == 0) {
                printf("Found ACPI USB xHCI Controller : %ls\n",deviceID);
                xHciDevInst = childDevInst;
                //break;
            }
        }
    
        // Recurse into this child node to find deeper USB devices
        EnumerateDevices(childDevInst);

        // Get the next sibling
        cr = CM_Get_Sibling(&childDevInst, childDevInst, 0);
        
    } while (cr == CR_SUCCESS);
}

int main() {
    DEVINST rootDevInst;
    CONFIGRET cr;

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
        if (counter == 10)
        {
            printf("Reach counter limit %d, stop the recan daemon\n", counter);
            break;
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);

    // Locate root devnode
    cr = CM_Locate_DevNode(&rootDevInst, NULL, CM_LOCATE_DEVNODE_NORMAL);
    if (cr != CR_SUCCESS) {
        printf("Failed to locate root devnode: 0x%.8x\n",cr);
        return 1;
    }

    printf("Enumerating USB devices...\n");
    EnumerateDevices(rootDevInst);
    return 0;
}
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
