#include <windows.h>
#include <tlhelp32.h>
#include <dbt.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

#define ID_EDIT 101
#define ID_BTN  102

std::atomic<bool> g_challengeActive(false);
std::chrono::steady_clock::time_point g_lastUnlockTime;
bool g_hasUnlockedOnce = false;

std::vector<std::string> forbiddenProcesses = {"taskmgr.exe", "cmd.exe", "powershell.exe", "regedit.exe"};

std::vector<std::string> forbiddenKeywords = {
    // Adult / Pornographic
    "porn", "pornhub", "xvideos", "xhamster", "redtube", "brazzers", "stripchat", 
    "chaturbate", "livejasmin", "youporn", "xnxx", "tube8", "spankbang", "eporner", 
    "hentai", "erotic", "escort", "nsfw", "gangbang", "orgasm", "fetish", "bdsm", 
    "orgy", "swinger", "masturbate", "pussy", "cock", "cumshot", "anal", "blowjob", 
    "creampie", "dildo", "vibrator", "slut", "whore", "prostitute", "bitch", 
    "facial", "deepthroat", "squirt", "lesbian", "gay porn", "boob", "dick", 
    "xxx", "sex", "nude", "adult", "milf", "camgirl", "onlyfans", "hardcore", 
    "softcore", "strip", "voyeur", "cuckold", "bukkake", "bondage", "dominatrix", 
    "incest", "rape", "guro", "scat", "peepshow", "striptease", "motherless",
    "efukt", "heavy-r", "camsoda", "bongacams", "myfreecams", "cumpilation",
    "handjob", "footjob", "rimjob", "facesitting", "zoophilia", "shemale",

    // Gambling / Casino
    "betting", "casino", "poker", "blackjack", "roulette", "slots", "bet365", 
    "jackpot", "lotto", "bet", "gambling", "baccarat", "craps", "stake", "rollbit", 
    "vulkanvegas", "blitz", "pokerstars", "betfair", "unibet", "spin", "crypto casino", 
    "crash game", "aviator", "draftkings", "fanduel", "betmgm", "caesars", "888casino", 
    "bovada", "betway", "1xbet", "pinnacle", "highroller", "pachinko", "scratchcard",

    // Gore / Self-Harm / Dangerous Content
    "gore", "blue whale", "suicide", "self-harm", "snuff", "execution", "mutilation", 
    "beheading", "dead body", "bestiality", "choking game", "momo challenge", 
    "watchpeopledie", "liveleak", "hanging", "overdose", "cutting", "pro-ana", "pro-mia",

    // Piracy / Warez
    "torrent", "piratebay", "warez", "crack", "keygen", "libgen", "sci-hub", 
    "annas-archive", "z-library", "fitgirl", "dodi-repacks", "rutracker", "1337x", 
    "rarbg", "nyaa", "limetorrents", "yts", "demonoid", "pixeldrain"
};

const std::string g_expectedAnswer = "Adam";
bool g_success = false;
HHOOK g_keyboardHook = NULL;
std::string g_currentReason = "";
std::mutex g_reasonMutex;

HBRUSH g_hBlackBrush = NULL;
HFONT g_hFontTitle = NULL;
HFONT g_hFontNormal = NULL;

// Stany klawiszy modyfikujących do stabilnego hooka
bool g_isAltPressed = false;
bool g_isCtrlPressed = false;
bool g_isWinPressed = false;
bool g_isShiftPressed = false;

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

bool containsWholeWord(const std::string& text, const std::string& word) {//charles DICKens......
    size_t pos = 0;
    while ((pos = text.find(word, pos)) != std::string::npos) {
        bool leftOk = (pos == 0) || !isalpha((unsigned char)text[pos - 1]);
        size_t rightPos = pos + word.length();
        bool rightOk = (rightPos == text.length()) || !isalpha((unsigned char)text[rightPos]);
        
        if (leftOk && rightOk) {
            return true;
        }
        pos += word.length();
    }
    return false;
}

bool IsInCooldown() {
    if (!g_hasUnlockedOnce) return false;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - g_lastUnlockTime).count();
    return elapsed < 20;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT*)lParam;
        if (p) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (p->vkCode == VK_MENU || p->vkCode == VK_LMENU || p->vkCode == VK_RMENU) g_isAltPressed = true;
                if (p->vkCode == VK_CONTROL || p->vkCode == VK_LCONTROL || p->vkCode == VK_RCONTROL) g_isCtrlPressed = true;
                if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) g_isWinPressed = true;
                if (p->vkCode == VK_SHIFT || p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT) g_isShiftPressed = true;
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (p->vkCode == VK_MENU || p->vkCode == VK_LMENU || p->vkCode == VK_RMENU) g_isAltPressed = false;
                if (p->vkCode == VK_CONTROL || p->vkCode == VK_LCONTROL || p->vkCode == VK_RCONTROL) g_isCtrlPressed = false;
                if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) g_isWinPressed = false;
                if (p->vkCode == VK_SHIFT || p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT) g_isShiftPressed = false;
            }

            // Niezawodna blokada klawiszy systemowych i skrótów
            if (g_isWinPressed) return 1;
            if (g_isAltPressed && p->vkCode == VK_TAB) return 1;
            if (g_isAltPressed && p->vkCode == VK_F4) return 1;
            if (g_isCtrlPressed && p->vkCode == VK_ESCAPE) return 1;
            if (g_isCtrlPressed && g_isShiftPressed && p->vkCode == VK_ESCAPE) return 1;
            if (g_isWinPressed && (p->vkCode == 'R' || p->vkCode == 'r')) return 1;
        }
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK ChallengeWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit;
    static HWND hButton;

    switch (uMsg) {
    case WM_CREATE: {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int w = 700, h = 340;
        int posX = (screenWidth - w) / 2;
        int posY = (screenHeight - h) / 2;

        HWND hStaticTitle = CreateWindowA("STATIC", "We have detected suspicious activity", 
            WS_CHILD | WS_VISIBLE | SS_CENTER, posX + 20, posY + 20, 660, 40, hwnd, NULL, NULL, NULL);
        SendMessage(hStaticTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        std::string localReason;
        {
            std::lock_guard<std::mutex> lock(g_reasonMutex);
            localReason = g_currentReason;
        }

        std::string fullMsg = "Reason: " + localReason + "\n\nYou have to enter password to continue";
        HWND hStaticReason = CreateWindowA("STATIC", fullMsg.c_str(), 
            WS_CHILD | WS_VISIBLE | SS_CENTER, posX + 20, posY + 70, 660, 80, hwnd, NULL, NULL, NULL);
        SendMessage(hStaticReason, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        hEdit = CreateWindowA("EDIT", "", 
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD, posX + 220, posY + 170, 260, 30, hwnd, (HMENU)ID_EDIT, NULL, NULL);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        hButton = CreateWindowA("BUTTON", "Submit", 
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, posX + 250, posY + 220, 200, 35, hwnd, (HMENU)ID_BTN, NULL, NULL);
        SendMessage(hButton, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        
        SetFocus(hEdit);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(0, 255, 0));
        SetBkColor(hdcStatic, RGB(0, 0, 0));
        return (INT_PTR)g_hBlackBrush;
    }
    case WM_CTLCOLORBTN: {
        return (INT_PTR)g_hBlackBrush;
    }
    case WM_TIMER:
        if (wParam == 1) {
            SetForegroundWindow(hwnd);
            BringWindowToTop(hwnd);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN) {
            char buffer[256];
            GetWindowTextA(hEdit, buffer, sizeof(buffer));
            std::string userAnswer(buffer);

            if (userAnswer == g_expectedAnswer) {
                g_success = true;
                DestroyWindow(hwnd);
            } else {
                MessageBoxA(hwnd, "Incorrect password! Try again.", "Verification Error", MB_OK | MB_ICONERROR);
                SetWindowTextA(hEdit, "");
                SetFocus(hEdit);
            }
        }
        break;
    case WM_CLOSE:
        if (!g_success) return 0; 
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

bool RunChallengeModal(const std::string& reason) {
    g_success = false;
    {
        std::lock_guard<std::mutex> lock(g_reasonMutex);
        g_currentReason = reason;
    }
    
    g_hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
    g_hFontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    g_hFontNormal = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = ChallengeWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "SecureChallengeClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBlackBrush;
    RegisterClassA(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        "SecureChallengeClass",
        "SYSTEM PROTECTION - 2FA",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    SetTimer(hwnd, 1, 100, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (g_success) break;
    }

    KillTimer(hwnd, 1);
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = NULL;
    }

    if (g_hBlackBrush) DeleteObject(g_hBlackBrush);
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);

    UnregisterClassA("SecureChallengeClass", GetModuleHandle(NULL));
    return g_success;
}

void TriggerSecurityEvent(const std::string& reason) {
    if (g_challengeActive || IsInCooldown()) return;
    g_challengeActive = true;

    bool unlocked = RunChallengeModal(reason);
    if (unlocked) {
        g_lastUnlockTime = std::chrono::steady_clock::now();
        g_hasUnlockedOnce = true;
    }
    g_challengeActive = false;
}

std::string CheckForbiddenProcesses() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return "";

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            std::string procName = toLower(pe.szExeFile);
            for (const auto& target : forbiddenProcesses) {
                if (procName == toLower(target)) {
                    CloseHandle(hSnapshot);
                    return "Unauthorized process execution detected: " + std::string(pe.szExeFile);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return "";
}

struct BrowserScanData {
    bool violationFound;
    std::string reason;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    BrowserScanData* data = (BrowserScanData*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;

    char windowTitle[256];
    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
    std::string titleStr(windowTitle);
    if (titleStr.empty()) return TRUE;

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        char buffer[MAX_PATH];
        if (GetModuleBaseNameA(hProcess, NULL, buffer, sizeof(buffer))) {
            std::string procName = toLower(buffer);
            std::vector<std::string> targetBrowsers = {"chrome.exe", "msedge.exe", "firefox.exe", "brave.exe", "tor.exe"};
            for (const auto& browser : targetBrowsers) {
                if (procName == browser) {
                    std::string lowerTitle = toLower(titleStr);
                    for (const auto& word : forbiddenKeywords) {
                        if (containsWholeWord(lowerTitle, toLower(word))) {
                            CloseHandle(hProcess);
                            data->violationFound = true;
                            data->reason = "Forbidden keyword found (" + word + ") in browser: \"" + titleStr + "\"";
                            return FALSE;
                        }
                    }
                }
            }
        }
        CloseHandle(hProcess);
    }
    return TRUE;
}

LRESULT CALLBACK DeviceWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DEVICECHANGE) {
        if (wParam == DBT_DEVICEARRIVAL) {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
            if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                TriggerSecurityEvent("New USB storage device connection detected.");
            }
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void StartDeviceListener() {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DeviceWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "USBListenerClass";
    RegisterClassA(&wc);

    CreateWindowExA(0, "USBListenerClass", "USB Listener", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int response = MessageBoxA(NULL, "Do you want to start the security helper?", "Security Helper Startup", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
    if (response != IDYES) {
        return 0;
    }

    std::thread usbThread(StartDeviceListener);
    usbThread.detach();

    while (true) {
        if (!g_challengeActive && !IsInCooldown()) {
            std::string procReason = CheckForbiddenProcesses();
            if (!procReason.empty()) {
                TriggerSecurityEvent(procReason);
                continue;
            }

            BrowserScanData scanData = {false, ""};
            EnumWindows(EnumWindowsProc, (LPARAM)&scanData);
            if (scanData.violationFound) {
                TriggerSecurityEvent(scanData.reason);
                continue;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}