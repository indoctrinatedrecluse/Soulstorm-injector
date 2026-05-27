#include <iostream>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>

// --- Logging Setup ---
std::ofstream logFile;

void Log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[100];
    ctime_s(buf, sizeof(buf), &time);
    std::string timeStr(buf);
    timeStr.erase(timeStr.length() - 1); // remove newline

    std::string logLine = "[" + timeStr + "] " + message;

    std::cout << logLine << std::endl;

    if (logFile.is_open()) {
        logFile << logLine << std::endl;
    }
}

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

// Function to get the absolute path of the DLL
std::string GetDllPath(const std::string& dllName) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos) + "\\" + dllName;
}

int main() {
    // Initialize logging
    std::string logPath = GetDllPath("injector.log");
    logFile.open(logPath, std::ios::out | std::ios::app);

    Log("--- Soulstorm Injector Started ---");

    // 1. Wait for and find process
    std::wstring targetProcessName = L"Soulstorm.exe";
    DWORD processId = 0;

    Log("Waiting for Soulstorm.exe process to start...");
    while (processId == 0) {
        processId = GetProcessIdByName(targetProcessName);
        if (processId == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    Log("Found Soulstorm.exe (PID: " + std::to_string(processId) + ")");

    // Give the game a moment to initialize if we just caught it launching
    Log("Waiting for process to initialize...");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 2. Open process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        Log("ERROR: Failed to open process. Error code: " + std::to_string(GetLastError()));
        Log("Tip: Ensure the injector is running as Administrator.");
        system("pause");
        return 1;
    }
    Log("Successfully opened process handle.");

    // Get the full path to our payload DLL
    std::string dllPath = GetDllPath("payload.dll");
    Log("Target DLL path: " + dllPath);

    // Verify DLL exists
    std::ifstream dllFile(dllPath);
    if(!dllFile.good()) {
        Log("ERROR: payload.dll not found at expected path!");
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // 3. Allocate memory in target process
    LPVOID pRemoteString = VirtualAllocEx(hProcess, NULL, dllPath.length() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pRemoteString == NULL) {
        Log("ERROR: Failed to allocate memory in target process. Error code: " + std::to_string(GetLastError()));
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    char addrBuf[64];
    sprintf_s(addrBuf, "0x%p", pRemoteString);
    Log("Allocated memory at " + std::string(addrBuf));

    // 4. Write DLL path to target process
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, pRemoteString, dllPath.c_str(), dllPath.length() + 1, &bytesWritten)) {
        Log("ERROR: Failed to write DLL path to target process. Error code: " + std::to_string(GetLastError()));
        VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    Log("Wrote DLL path to target memory.");

    // 5. Get address of LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");
    if (pLoadLibrary == NULL) {
        Log("ERROR: Failed to get address of LoadLibraryA. Error code: " + std::to_string(GetLastError()));
        VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // 6. Create remote thread
    Log("Creating remote thread to load DLL...");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemoteString, 0, NULL);
    if (hThread == NULL) {
        Log("ERROR: Failed to create remote thread. Error code: " + std::to_string(GetLastError()));
        VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // Wait for the thread to finish
    WaitForSingleObject(hThread, INFINITE);
    DWORD exitCode;
    GetExitCodeThread(hThread, &exitCode);

    if (exitCode == 0) {
        Log("WARNING: LoadLibraryA might have failed (Exit code 0). Check game architecture (x86 vs x64).");
    } else {
        Log("Injection completed successfully. Payload is active.");
    }

    // Clean up injection handles
    VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
    CloseHandle(hThread);

    // Monitor the game process
    Log("Monitoring game process. Injector will close automatically when the game does.");
    WaitForSingleObject(hProcess, INFINITE);

    CloseHandle(hProcess);
    Log("Game process closed.");
    Log("--- Injector Shutting Down ---");

    if (logFile.is_open()) logFile.close();

    return 0;
}
