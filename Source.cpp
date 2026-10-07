#include "pch.h"
#include <windows.h>
#include <string>
#include <algorithm>
#include <cwchar>
#include <cstring>

bool IsMinecraftForeground()
{
    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    wchar_t title[256] = {};
    if (GetWindowTextW(foreground, title, ARRAYSIZE(title)) != 0)
    {
        std::wstring windowTitle(title);
        std::transform(windowTitle.begin(), windowTitle.end(), windowTitle.begin(), ::towlower);
        if (windowTitle.find(L"minecraft") != std::wstring::npos)
            return true;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    if (!processId)
        return false;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
        return false;

    wchar_t imagePath[MAX_PATH] = {};
    DWORD pathLength = MAX_PATH;
    const bool hasImage = QueryFullProcessImageNameW(process, 0, imagePath, &pathLength) != 0;
    CloseHandle(process);

    if (!hasImage)
        return false;

    std::wstring imageName(imagePath, pathLength);
    std::transform(imageName.begin(), imageName.end(), imageName.begin(), ::towlower);
    return imageName.find(L"minecraft") != std::wstring::npos ||
           imageName.find(L"javaw.exe") != std::wstring::npos;
}

bool PressKey(WORD virtualKey)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = virtualKey;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT)) == ARRAYSIZE(inputs);
}

bool PasteShortcut()
{
    INPUT controlDown = {};
    controlDown.type = INPUT_KEYBOARD;
    controlDown.ki.wVk = VK_CONTROL;
    if (SendInput(1, &controlDown, sizeof(INPUT)) != 1)
        return false;

    Sleep(20);
    const bool pasted = PressKey('V');

    INPUT controlUp = {};
    controlUp.type = INPUT_KEYBOARD;
    controlUp.ki.wVk = VK_CONTROL;
    controlUp.ki.dwFlags = KEYEVENTF_KEYUP;
    return pasted && SendInput(1, &controlUp, sizeof(INPUT)) == 1;
}

bool SetClipboardText(const wchar_t* text)
{
    if (!OpenClipboard(nullptr))
        return false;

    const SIZE_T bytes = (std::wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory)
    {
        CloseClipboard();
        return false;
    }

    void* buffer = GlobalLock(memory);
    if (!buffer)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    std::memcpy(buffer, text, bytes);
    GlobalUnlock(memory);

    if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory))
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

bool SendCommand(const wchar_t* command)
{
    HWND minecraftWindow = GetForegroundWindow();
    if (!minecraftWindow || !IsMinecraftForeground())
        return false;

    if (!SetClipboardText(command))
        return false;

    if (!PressKey('T'))
        return false;
    Sleep(50);

    if (GetForegroundWindow() != minecraftWindow)
        return false;

    if (!PasteShortcut())
        return false;
    Sleep(50);

    if (GetForegroundWindow() != minecraftWindow)
        return false;

    return PressKey(VK_RETURN);
}

void SendCommands()
{
    if (SendCommand(L"/home"))
        SendCommand(L"/desert");
}

DWORD WINAPI MainThread(LPVOID moduleHandle)
{
    bool wasDown = false;

    while (true)
    {
        if ((GetAsyncKeyState(VK_END) & 0x8000) != 0)
            FreeLibraryAndExitThread(static_cast<HMODULE>(moduleHandle), 0);

        const bool down = (GetAsyncKeyState('R') & 0x8000) != 0;
        if (down && !wasDown && IsMinecraftForeground())
            SendCommands();

        wasDown = down;
        Sleep(10);
    }

    return 0;
}