#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <locale>
#include <codecvt>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <comdef.h>
#include <taskschd.h>
#include <map>
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

void slowPrint(const std::string& text, unsigned int ms_per_char) {
    for (const char c : text) {
        std::cout << c;
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(ms_per_char));
    }
}

void showASCIIArt() {

    unsigned int speed = 10;

    slowPrint(" _  _       _        __  __            _             \n", speed);
    slowPrint("| || | ___ | | _ __ |  \\/  | __ _  ___| |_  ___  _ _ \n", speed);
    slowPrint("| __ |/ -_)| || '_ \\| |\\/| |/ _` |(_-<|  _|/ -_)| '_|\n", speed);
    slowPrint("|_||_|\\___||_|| .__/|_|  |_|\\__,_|/__/ \\__|\\___||_|  \n", speed);
    slowPrint("              |_|                                     \n", speed);
    slowPrint("                                                       \n", speed);
    slowPrint("            version: 1.0(advanced)                      \n", speed);
    slowPrint("            author(discord): hozdry \n", speed);

}

void preventWindowClose() {
    HWND hWnd = GetConsoleWindow();
    LONG style = GetWindowLong(hWnd, GWL_STYLE);
    style &= ~WS_SYSMENU;
    SetWindowLong(hWnd, GWL_STYLE, style);

    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void manageTaskManager(bool enable) {
    std::string command = "reg add \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\" /v DisableTaskMgr /t REG_DWORD /d ";
    command += (enable ? "0" : "1");
    command += " /f";
    system(command.c_str());
    std::wcout << L"\nДиспетчер задач " << (enable ? L"включен" : L"отключен") << L".\n";
}

void readRegistryValues(HKEY hKey, const std::wstring& path) {
    HKEY hKeyOpened;
    if (RegOpenKeyEx(hKey, path.c_str(), 0, KEY_READ, &hKeyOpened) == ERROR_SUCCESS) {
        DWORD dwValueCount;
        if (RegQueryInfoKey(hKeyOpened, NULL, NULL, NULL, NULL, NULL, NULL, &dwValueCount, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < dwValueCount; i++) {
                WCHAR szValueName[256];
                DWORD dwValueNameSize = 256;
                if (RegEnumValue(hKeyOpened, i, szValueName, &dwValueNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                    WCHAR szValueData[256];
                    DWORD dwValueDataSize = 256;
                    if (RegQueryValueEx(hKeyOpened, szValueName, NULL, NULL, (LPBYTE)szValueData, &dwValueDataSize) == ERROR_SUCCESS) {
                        std::wcout << L"Приложение: " << szValueName << L" = путь:" << szValueData << std::endl;
                    }
                }
            }
        }
        RegCloseKey(hKeyOpened);
    }
}

void showRegistryValues() {
    std::wstring path1 = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring path2 = L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring path3 = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    std::wcout << L"HKEY_LOCAL_MACHINE\\" << path1 << std::endl;
    readRegistryValues(HKEY_LOCAL_MACHINE, path1);
    std::wcout << std::endl;

    std::wcout << L"HKEY_LOCAL_MACHINE\\" << path2 << std::endl;
    readRegistryValues(HKEY_LOCAL_MACHINE, path2);
    std::wcout << std::endl;

    std::wcout << L"HKEY_CURRENT_USER\\" << path3 << std::endl;
    readRegistryValues(HKEY_CURRENT_USER, path3);
}

std::vector<PROCESSENTRY32> listProcesses() {
    std::vector<PROCESSENTRY32> processes;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Ошибка: не удалось создать список процессов." << std::endl;
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            processes.push_back(pe32);
        } while (Process32Next(hProcessSnap, &pe32));
    }
    else {
        std::wcerr << L"Ошибка: не удалось получить информацию о процессах." << std::endl;
    }

    CloseHandle(hProcessSnap);
    return processes;
}

void suspendProcess(DWORD processID) {
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Ошибка: не удалось создать снимок потоков." << std::endl;
        return;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hThreadSnap, &te32)) {
        do {
            if (te32.th32OwnerProcessID == processID) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread == NULL) {
                    std::wcerr << L"Ошибка: не удалось открыть поток с ID " << te32.th32ThreadID << L"." << std::endl;
                    continue;
                }
                SuspendThread(hThread);
                CloseHandle(hThread);
            }
        } while (Thread32Next(hThreadSnap, &te32));
        std::wcout << L"Процесс с ID " << processID << L" приостановлен." << std::endl;
    }
    else {
        std::wcerr << L"Ошибка: не удалось получить информацию о потоках." << std::endl;
    }

    CloseHandle(hThreadSnap);
}

void terminateProcess(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
    if (hProcess == NULL) {
        std::wcerr << L"Ошибка: не удалось открыть процесс с ID " << processID << L"." << std::endl;
        return;
    }

    if (TerminateProcess(hProcess, 0)) {
        std::wcout << L"Процесс с ID " << processID << L" завершен." << std::endl;
    }
    else {
        std::wcerr << L"Ошибка: не удалось завершить процесс с ID " << processID << L"." << std::endl;
    }

    CloseHandle(hProcess);
}

void manageProcess() {
    auto processes = listProcesses();
    if (processes.empty()) {
        return;
    }

    std::wcout << L"\nДоступные процессы:\n";
    for (const auto& proc : processes) {
        std::wcout << L"ID: " << proc.th32ProcessID << L", Название: " << proc.szExeFile << std::endl;
    }

    DWORD processID;
    std::wcout << L"\nВведите ID процесса, которым хотите управлять: ";
    std::wcin >> processID;

    std::wstring action;
    std::wcout << L"\n[1] Заморозить процесс\n[2] Завершить процесс\n[3] Вернуться в меню\nВыберите опцию: ";
    std::wcin >> action;

    if (action == L"1") {
        suspendProcess(processID);
    }
    else if (action == L"2") {
        terminateProcess(processID);
    }
    else if (action == L"3") {
        return;
    }
    else {
        std::wcout << L"\nНеправильный выбор, попробуйте снова.\n";
    }
}

struct Task {
    std::wstring name;
    int id;
};

std::map<int, Task> tasks;
int taskIdCounter = 1;

void deleteScheduledTask(const std::wstring& taskName) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"Ошибка инициализации COM." << std::endl;
        return;
    }

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&pService);
    if (SUCCEEDED(hr)) {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr)) {
            ITaskFolder* pRootFolder = NULL;
            hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
            if (SUCCEEDED(hr)) {
                hr = pRootFolder->DeleteTask(_bstr_t(taskName.c_str()), 0);
                if (SUCCEEDED(hr)) {
                    std::wcout << L"Задача '" << taskName << L"' удалена." << std::endl;
                }
                else {
                    std::wcerr << L"Ошибка удаления задачи: " << _com_error(hr).ErrorMessage() << std::endl;
                }
                pRootFolder->Release();
            }
        }
        pService->Release();
    }

    CoUninitialize();
}

void listScheduledTasks() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::wcerr << L"Ошибка инициализации COM." << std::endl;
        return;
    }

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&pService);
    if (SUCCEEDED(hr)) {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr)) {
            ITaskFolder* pRootFolder = NULL;
            hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
            if (SUCCEEDED(hr)) {
                IRegisteredTaskCollection* pTaskCollection = NULL;
                hr = pRootFolder->GetTasks(0, &pTaskCollection);
                if (SUCCEEDED(hr)) {
                    LONG taskCount = 0;
                    pTaskCollection->get_Count(&taskCount);
                    for (LONG i = 1; i <= taskCount; i++) {
                        IRegisteredTask* pTask = NULL;
                        hr = pTaskCollection->get_Item(_variant_t(i), &pTask);
                        if (SUCCEEDED(hr)) {
                            BSTR bstrName;
                            pTask->get_Name(&bstrName);
                            std::wcout << L"Запланированная задача: " << bstrName << L" (ID: " << taskIdCounter << L")" << std::endl;
                            tasks[taskIdCounter++] = { bstrName, taskIdCounter - 1 };
                            SysFreeString(bstrName);
                            pTask->Release();
                        }
                        else {
                            std::wcerr << L"Ошибка при получении задачи с индексом " << i << L"." << std::endl;
                        }
                    }
                    pTaskCollection->Release();
                }
                else {
                    std::wcerr << L"Ошибка получения задач." << std::endl;
                }
                pRootFolder->Release();
            }
            else {
                std::wcerr << L"Ошибка получения корневой папки." << std::endl;
            }
        }
        else {
            std::wcerr << L"Ошибка подключения к сервису задач." << std::endl;
        }
        pService->Release();
    }
    else {
        std::wcerr << L"Ошибка создания экземпляра сервиса задач." << std::endl;
    }

    CoUninitialize();

    int option;
    std::wcout << L"\n[1] Удалить задачу\n[2] Выйти в меню\nВыберите опцию: ";
    std::wcin >> option;

    if (option == 1) {
        int taskId;
        std::wcout << L"Введите ID задачи: ";
        std::wcin >> taskId;
        auto it = tasks.find(taskId);
        if (it != tasks.end()) {
            deleteScheduledTask(it->second.name);
            tasks.erase(it);
        }
        else {
            std::wcout << L"Задача с указанным ID не найдена." << std::endl;
        }
    }
}

int main() {
    preventWindowClose();

    std::locale::global(std::locale("ru_RU.UTF-8"));
    std::wcout.imbue(std::locale());

    showASCIIArt();

    std::wstring choice;
    while (true) {
        std::wcout << L"\n[1] Отключить диспетчер задач\n[2] Включить диспетчер задач\n[3] Показать автозагрузку\n[4] Показать запланированные задачи\n[5] Управление процессами\n[6] Выйти\nВыберите опцию: ";
        std::wcin >> choice;

        if (choice == L"1") {
            manageTaskManager(false);
        }
        else if (choice == L"2") {
            manageTaskManager(true);
        }
        else if (choice == L"3") {
            showRegistryValues();
        }
        else if (choice == L"4") {
            listScheduledTasks();
        }
        else if (choice == L"5") {
            manageProcess();
        }
        else if (choice == L"6") {
            break;
        }
        else {
            std::wcout << L"\nНеправильный выбор, попробуйте снова.\n";
        }
    }

    return 0;
}
