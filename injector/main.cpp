#include <iostream>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <chrono>
#include <thread>

// Function to find process ID by executable name
DWORD GetProcessIdByName(const std::wstring& processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(processEntry);

        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (!_wcsicmp(processEntry.szExeFile, processName.c_str())) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return processId;
}

// Function to get the absolute path of the DLL (assuming it's in the same build directory)
std::string GetDllPath(const std::string& dllName) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos) + "\\" + dllName;
}

int main() {
    std::cout << "Soulstorm Injector" << std::endl;
    std::cout << "==================" << std::endl;

    // 1. Wait for and find process (Soulstorm.exe)
    std::wstring targetProcessName = L"Soulstorm.exe";
    DWORD processId = 0;

    std::cout << "[+] Waiting for Soulstorm.exe..." << std::endl;
    while (processId == 0) {
        processId = GetProcessIdByName(targetProcessName);
        if (processId == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    std::cout << "[+] Found process ID: " << processId << std::endl;

    // 2. Open process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "[-] Failed to open process. Error code: " << GetLastError() << std::endl;
        std::cerr << "[-] Try running the injector as Administrator." << std::endl;
        system("pause");
        return 1;
    }
    std::cout << "[+] Opened process successfully." << std::endl;

    // Get the full path to our payload DLL
    std::string dllPath = GetDllPath("payload.dll");
    std::cout << "[+] DLL Path: " << dllPath << std::endl;

    // 3. Allocate memory in target process for DLL path
    LPVOID pRemoteString = VirtualAllocEx(hProcess, NULL, dllPath.length() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pRemoteString == NULL) {
        std::cerr << "[-] Failed to allocate memory in target process. Error code: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    std::cout << "[+] Allocated memory for DLL path at: 0x" << std::hex << pRemoteString << std::dec << std::endl;

    // 4. Write DLL path to target process
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, pRemoteString, dllPath.c_str(), dllPath.length() + 1, &bytesWritten)) {
        std::cerr << "[-] Failed to write DLL path to target process. Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    std::cout << "[+] Wrote " << bytesWritten << " bytes to target process." << std::endl;

    // 5. Get address of LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");
    if (pLoadLibrary == NULL) {
        std::cerr << "[-] Failed to get address of LoadLibraryA. Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // 6. Create remote thread to execute LoadLibraryA with the DLL path
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemoteString, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "[-] Failed to create remote thread. Error code: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    std::cout << "[+] Injection completed successfully." << std::endl;

    // Clean up injection handles
    VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
    CloseHandle(hThread);

    // Monitor the game process. If it closes, we exit.
    std::cout << "[+] Monitoring game process. Injector will close automatically when the game does." << std::endl;
    WaitForSingleObject(hProcess, INFINITE);

    CloseHandle(hProcess);
    std::cout << "[+] Game process closed. Exiting." << std::endl;

    return 0;
}
