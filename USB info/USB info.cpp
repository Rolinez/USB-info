#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <tchar.h>    
#include <Windows.h>
#include <Setupapi.h>
#include <winusb.h>

#undef LowSpeed
#include <Usbioctl.h>

#include <stdlib.h>
#include <Devpkey.h>
#include <iostream>
#include <string>
#include <memory>
#include <strsafe.h>

#pragma comment(lib,"Setupapi.lib")


void ErrorMes(LPTSTR lpszFunction)
{
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    wprintf(L"%s failed with error %d: %s",
        lpszFunction, dw, lpMsgBuf);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);

}

typedef struct {
    wchar_t name[1024];
    wchar_t id[1024];

} USB_DEVICE_PARAMS;

BOOL GetDevice(wchar_t* id, wchar_t* output)
{
    unsigned index;
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    TCHAR id_upper[1024] = L"";
    TCHAR buf[1024] = L"";
    TCHAR match[1024];
    DEVPROPTYPE dpt = 0;


    for (int i = 0;i < wcslen(id);i++) {
        id_upper[i] = toupper(id[i]);
    }

    hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    for (index = 0; ; index++) {
        DeviceInfoData.cbSize = sizeof(DeviceInfoData);
        if (!SetupDiEnumDeviceInfo(hDevInfo, index, &DeviceInfoData)) {
            return FALSE;
        }

        BOOL res = SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData,
            &DEVPKEY_Device_InstanceId, &dpt, (PBYTE)buf, 1000, NULL, 0);
        if (res == FALSE)continue;


        if (wcscmp(buf, id_upper) == 0) {
            res = SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData,
                &DEVPKEY_Device_FriendlyName, &dpt, (PBYTE)buf, 1000, NULL, 0);

            wcscpy(output, buf);

            return TRUE;
        }
    }
    return FALSE;

}

BOOL GetMassStorageDevice(int vid, int pid, USB_DEVICE_PARAMS* output)
{
    unsigned index;
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    TCHAR HardwareID[1024];
    TCHAR buf[1024];
    TCHAR match[1024];
    DEVPROPTYPE dpt = 0;
    BOOL res;

    StringCchPrintf(match, 1024, L"VID_%04X&PID_%04X", vid, pid);

    hDevInfo = SetupDiGetClassDevs(NULL, TEXT("USB"), NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    for (index = 0; ; index++) {
        DeviceInfoData.cbSize = sizeof(DeviceInfoData);
        if (!SetupDiEnumDeviceInfo(hDevInfo, index, &DeviceInfoData)) {
            return FALSE; 
        }

        res = SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_HARDWAREID, NULL,
            (BYTE*)HardwareID, sizeof(HardwareID), NULL);
        if (res == FALSE)continue;

        if (_tcsstr(HardwareID, match)) {
            res = SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_SERVICE, NULL,
                (BYTE*)buf, sizeof(buf), NULL);
            if (res == FALSE)continue;

            if (wcscmp(buf, L"USBSTOR") == 0) {
                res = SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC, NULL,
                    (BYTE*)buf, sizeof(buf), NULL);
                if (res == FALSE)continue;

                res = SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData,
                    &DEVPKEY_Device_Children, &dpt, (PBYTE)buf, 1000, NULL, 0);
                if (res == FALSE)continue;

                wcscpy(output->id, buf);

                GetDevice(buf, output->name);

                return TRUE;
            }
        }
    }
    return FALSE;

}

int main()
{
    setlocale(LC_ALL, "Russian");
    GUID guid;
    HRESULT hr = CLSIDFromString(L"{F18A0E88-C30C-11D0-8815-00A0C906BED8}", (LPCLSID)&guid);
    HDEVINFO deviceInfoHandle = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoHandle != INVALID_HANDLE_VALUE)
    {
        int deviceIndex = 0;
        while (true)
        {
            SP_DEVICE_INTERFACE_DATA deviceInterface = { 0 };
            deviceInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
            if (SetupDiEnumDeviceInterfaces(deviceInfoHandle, 0, &guid, deviceIndex, &deviceInterface))
            {
                DWORD cbRequired = 0;

                SetupDiGetDeviceInterfaceDetail(deviceInfoHandle, &deviceInterface, 0, 0, &cbRequired, 0);
                if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
                {
                    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetail =
                        (PSP_DEVICE_INTERFACE_DETAIL_DATA)(new char[cbRequired]);
                    memset(deviceInterfaceDetail, 0, cbRequired);
                    deviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                    if (!SetupDiGetDeviceInterfaceDetail(deviceInfoHandle, &deviceInterface,
                        deviceInterfaceDetail, cbRequired, &cbRequired, 0))
                    {
                        deviceIndex++;
                        continue;
                    }

                    memset(deviceInterfaceDetail, 0, cbRequired);
                    deviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                    BOOL status = SetupDiGetDeviceInterfaceDetail(
                        deviceInfoHandle,
                        &deviceInterface, 
                        deviceInterfaceDetail,
                        cbRequired,  
                        &cbRequired,  
                        NULL);        

                    BOOL res;

                    HANDLE handle = CreateFile(deviceInterfaceDetail->DevicePath, GENERIC_WRITE, FILE_SHARE_WRITE,
                        0, OPEN_EXISTING, 0, 0);

                    if (handle != INVALID_HANDLE_VALUE) {
                        DWORD bytes_read = 0;
                        USB_HUB_INFORMATION_EX hubinfo;
                        hubinfo.HighestPortNumber = 0;

                        res = DeviceIoControl(handle, IOCTL_USB_GET_HUB_INFORMATION_EX,
                            &hubinfo, sizeof(hubinfo), &hubinfo, sizeof(hubinfo), &bytes_read, 0);

                        USB_NODE_CONNECTION_INFORMATION_EX coninfo = { 0 };
                        USB_NODE_CONNECTION_INFORMATION_EX_V2 con2 = { 0 };

                        for (int j = 1;j <= (int)hubinfo.HighestPortNumber;j++) {

                            coninfo.ConnectionIndex = j;
                            res = DeviceIoControl(handle, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                &coninfo, sizeof(coninfo), &coninfo, sizeof(coninfo), &bytes_read, 0);

                            if (coninfo.ConnectionStatus == 0)continue;
                            USB_DEVICE_PARAMS usbdev = { 0 };

                            if (GetMassStorageDevice(coninfo.DeviceDescriptor.idVendor,
                                coninfo.DeviceDescriptor.idProduct, &usbdev) != FALSE)
                            {
                                printf("\n- Хаб %2d, Порт %2d: USB v%x устройство\n", deviceIndex,
                                    j, (int)coninfo.DeviceDescriptor.bcdUSB);
                                printf("VID_%04X PID_%04X\n", (int)coninfo.DeviceDescriptor.idVendor
                                    , (int)coninfo.DeviceDescriptor.idProduct);
                                wprintf(L"ID устройства: %s\n", usbdev.id);
                                wprintf(L"Название устройства: %s\n", usbdev.name);
                                printf("Скорость: %d", (int)coninfo.Speed);

                                switch ((int)coninfo.Speed) {
                                case UsbLowSpeed:printf(" (low)\n");break;
                                case UsbFullSpeed:printf(" (full)\n");break;
                                case UsbHighSpeed:printf(" (high)\n");break;
                                case UsbSuperSpeed:printf(" (super)\n");break;
                                default:printf("\n");break;
                                }
                                con2.ConnectionIndex = j;
                                con2.Length = sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2);
                                con2.SupportedUsbProtocols.Usb300 = 1;

                                res = DeviceIoControl(handle, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2,
                                    &con2, sizeof(con2), &con2, sizeof(con2), &bytes_read, 0);

                                printf("Поддерживаемые протоколы: ");
                                if (con2.SupportedUsbProtocols.Usb110)printf("USB 1.1; ");
                                if (con2.SupportedUsbProtocols.Usb200)printf("USB 2.0; ");
                                if (con2.SupportedUsbProtocols.Usb300)printf("USB 3.0; ");
                                printf("\n");
                            }
                        }
                        CloseHandle(handle);
                    }
                    delete[] deviceInterfaceDetail;
                }
            }
            else
            {
                break;
            }
            ++deviceIndex;
        }
        SetupDiDestroyDeviceInfoList(deviceInfoHandle);
    }
    system("PAUSE");
    return 0;
}