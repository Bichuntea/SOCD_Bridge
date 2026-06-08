/**
 * @file    SOCD_Bridge.cpp
 * @brief   SOCD按键桥接器主程序
 * @details 解决同方向按键冲突（SOCD - Simultaneous Opposing Cardinal Directions）
 *          通过按键映射和延迟机制，优化游戏按键体验
 * @author  Bichuntea
 * @date    2026-05-27
 * @version 1.1
 * @license MIT
 * @link    https://github.com/Bichuntea/SOCD_Bridge
 *
 * @copyright Copyright (c) 2026 Bichuntea. All rights reserved.
 */

/* =========================================================================
 *                          包含头文件
 * ========================================================================= */
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <regex>
#include <vector>
#include <filesystem>
#include <ctime>
#include <algorithm>

/* =========================================================================
 *                          命名空间
 * ========================================================================= */
using namespace std;
namespace fs = std::filesystem;

/* =========================================================================
 *                          常量定义
 * ========================================================================= */

/** @name 系统托盘菜单ID */
/** @{ */
#define ID_TRAY_APP_ICON                1001    /**< 托盘图标ID */
#define ID_TRAY_EXIT_CONTEXT_MENU_ITEM  3000    /**< 退出菜单项 */
#define ID_TRAY_VERSION_INFO            3001    /**< 版本信息菜单项 */
#define ID_TRAY_REBIND_KEYS             3002    /**< 重新绑定按键菜单项 */
#define ID_TRAY_LOCK_FUNCTION           3003    /**< 锁定/解锁功能菜单项 */
#define ID_TRAY_RESTART_SOCD_BRIDGE     3004    /**< 重启程序菜单项 */
#define ID_TRAY_HELP                    3005    /**< 帮助菜单项 */
#define ID_TRAY_LAYOUTS                 3007    /**< 布局菜单项 */
#define ID_TRAY_DELAY_SETTINGS          3008    /**< 延迟设置菜单项 */
/** @} */

/** @name 版本信息对话框控件ID */
/** @{ */
#define ID_VERSION_SYSLINK              5100    /**< SysLink控件ID */
#define ID_VERSION_BUTTON_OK            5101    /**< 确定按钮ID */
/** @} */

/** @name 自定义消息 */
/** @{ */
#define WM_TRAYICON                     (WM_USER + 1)   /**< 托盘图标消息 */
#define WM_NOTIFY_LINKCLICK             (WM_USER + 100) /**< 链接点击消息 */
/** @} */

/** @name 布局菜单ID基准值 */
#define ID_LAYOUT_BASE                  4000    /**< 布局菜单项起始ID */

/** @name 延迟设置对话框控件ID */
/** @{ */
#define ID_DELAY_CHECKBOX_ENABLE        5001    /**< 启用延迟复选框 */
#define ID_DELAY_SLIDER_DELAY           5002    /**< 延迟时间滑块 */
#define ID_DELAY_EDIT_DELAY             5003    /**< 延迟时间输入框 */
#define ID_DELAY_SLIDER_FLOAT           5004    /**< 浮动范围滑块 */
#define ID_DELAY_EDIT_FLOAT             5005    /**< 浮动范围输入框 */
#define ID_DELAY_BUTTON_OK              5006    /**< 确定按钮 */
#define ID_DELAY_BUTTON_CANCEL          5007    /**< 取消按钮 */
#define ID_DELAY_LABEL_DELAY_VALUE      5008    /**< 延迟时间显示标签 */
#define ID_DELAY_LABEL_FLOAT_VALUE      5009    /**< 浮动范围显示标签 */
#define ID_DELAY_BUTTON_APPLY           5010    /**< 应用按钮 */
#define ID_DELAY_BUTTON_CONFIRM_DELAY   5011    /**< 延迟时间确认按钮 */
#define ID_DELAY_BUTTON_CONFIRM_FLOAT   5012    /**< 浮动范围确认按钮 */
#define ID_DELAY_BUTTON_RESET           5013    /**< 恢复默认按钮 */
/** @} */

/** @name 延迟设置参数范围 */
/** @{ */
#define DEFAULT_DELAY_MS                40      /**< 默认延迟时间(ms) */
#define DEFAULT_FLOAT_RANGE_MS          20      /**< 默认浮动范围(ms) */
#define MIN_DELAY_MS                    8       /**< 最小延迟时间(ms) */
#define MAX_DELAY_MS                    200     /**< 最大延迟时间(ms) */
#define MIN_FLOAT_RANGE_MS              1       /**< 最小浮动范围(ms) */
#define MAX_FLOAT_RANGE_MS              50      /**< 最大浮动范围(ms) */
/** @} */

/** @name 对话框尺寸 */
/** @{ */
#define DIALOG_WIDTH                    430     /**< 对话框宽度 */
#define DIALOG_HEIGHT                   290     /**< 对话框高度 */
/** @} */

/* =========================================================================
 *                          数据结构定义
 * ========================================================================= */

/**
 * @brief 按键状态结构体
 * @details 记录每个按键的注册状态、按下状态、所属组和模拟标志
 */
struct KeyState {
    bool registered = false;    /**< 按键是否已注册 */
    bool keyDown = false;       /**< 按键是否处于按下状态 */
    int group = 0;              /**< 按键所属的分组ID */
    bool simulated = false;     /**< 是否为模拟按键 */
};

/**
 * @brief 按键组状态结构体
 * @details 记录每组按键中当前激活的按键和上一个按键
 */
struct GroupState {
    int previousKey = 0;        /**< 上一个激活的按键 */
    int activeKey = 0;          /**< 当前激活的按键 */
};

/**
 * @brief 延迟设置结构体
 * @details 存储输入延迟功能的配置参数
 */
struct DelaySettings {
    bool enabled = false;               /**< 是否启用延迟功能 */
    int delayMs = DEFAULT_DELAY_MS;     /**< 延迟时间(ms) */
    int floatRangeMs = DEFAULT_FLOAT_RANGE_MS; /**< 浮动范围(ms) */
};

/* =========================================================================
 *                          全局变量
 * ========================================================================= */

/** @name 按键状态映射表 */
/** @{ */
unordered_map<int, GroupState> g_groupInfo;     /**< 按键组状态映射 */
unordered_map<int, KeyState> g_keyInfo;         /**< 按键状态映射 */
/** @} */

/** @name 延迟设置相关 */
/** @{ */
DelaySettings g_delaySettings;          /**< 延迟设置实例 */
HWND g_hDelayDialog = NULL;             /**< 延迟设置对话框句柄 */
/** @} */

/** @name 系统相关全局变量 */
/** @{ */
HHOOK g_hHook = NULL;                   /**< 键盘钩子句柄 */
HANDLE g_hMutex = NULL;                 /**< 互斥量句柄 */
NOTIFYICONDATA g_nid;                   /**< 托盘图标数据 */
bool g_isLocked = false;                /**< 功能锁定状态 */
/** @} */

/* =========================================================================
 *                          前向声明
 * ========================================================================= */

/** @name 窗口过程函数 */
/** @{ */
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DelayDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
/** @} */

/** @name 初始化函数 */
/** @{ */
void InitNotifyIconData(HWND hwnd);
/** @} */

/** @name 配置管理函数 */
/** @{ */
bool LoadConfig(const string& filename);
void CreateDefaultConfig(const string& filename);
void RestoreConfigFromBackup(const string& backupFilename, const string& destinationFilename);
/** @} */

/** @name 布局管理函数 */
/** @{ */
vector<string> ListLayouts();
void ApplyLayout(const string& layoutName);
/** @} */

/** @name 按键处理函数 */
/** @{ */
void HandleKeyDown(int keyCode);
void HandleKeyUp(int keyCode);
void SendKey(int targetKey, bool keyDown);
bool IsSimulatedKeyEvent(DWORD flags);
int GetRandomDelay();
/** @} */

/** @name 工具函数 */
/** @{ */
string GetVersionInfo();
void OpenGitHubPage();
/** @} */

/** @name 配置文件管理函数 */
/** @{ */
void LoadDelayConfig();
void SaveDelayConfig();
/** @} */

/** @name 对话框函数 */
/** @{ */
void ShowDelaySettingsDialog(HWND hwndParent);
void ShowVersionDialog(HWND hwndParent);
void RestartSOCDBridge();
/** @} */

/* =========================================================================
 *                          函数实现 - 布局管理
 * ========================================================================= */

/**
 * @brief 获取所有可用的布局配置文件列表
 * @return 布局名称的字符串向量
 * @note 从 meta/profiles 目录中读取所有 .cfg 文件
 */
vector<string> ListLayouts() {
    vector<string> layouts;
    const string path = "meta\\profiles";
    
    if (!fs::exists(path)) {
        return layouts;
    }

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            const auto ext = entry.path().extension().string();
            if (ext == ".cfg") {
                layouts.push_back(entry.path().stem().string());
            }
        }
    }
    
    return layouts;
}

/**
 * @brief 应用指定的布局配置
 * @param[in] layoutName 布局名称（不含扩展名）
 * @note 将选中的布局文件内容复制到 config.cfg
 */
void ApplyLayout(const string& layoutName) {
    const string sourcePath = "meta\\profiles\\" + layoutName + ".cfg";
    const string destPath = "config.cfg";

    ifstream src(sourcePath, ios::binary);
    ofstream dst(destPath, ios::binary | ios::trunc);

    if (!src.is_open() || !dst.is_open()) {
        MessageBox(NULL, 
                   TEXT("应用布局失败，请检查布局文件。"),
                   TEXT("SOCD_Bridge 错误"), 
                   MB_ICONERROR | MB_OK);
        return;
    }

    dst << src.rdbuf();
}

/* =========================================================================
 *                          函数实现 - 程序控制
 * ========================================================================= */

/**
 * @brief 重启SOCD_Bridge程序
 * @note 启动新实例并退出当前实例
 */
void RestartSOCDBridge() {
    TCHAR szExeFileName[MAX_PATH];
    GetModuleFileName(NULL, szExeFileName, MAX_PATH);
    ShellExecute(NULL, NULL, szExeFileName, NULL, NULL, SW_SHOWNORMAL);
    PostQuitMessage(0);
}

/* =========================================================================
 *                          函数实现 - 主入口
 * ========================================================================= */

/**
 * @brief 程序主入口函数
 * @return 程序退出码，0表示成功
 * @note 初始化配置、创建窗口、安装键盘钩子、进入消息循环
 */
int main() {
    srand(static_cast<unsigned int>(time(NULL)));
    
    // 初始化通用控件（用于TrackBar等控件）
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // 加载配置文件
    if (!LoadConfig("config.cfg")) {
        return 1;
    }
    
    // 加载延迟配置
    LoadDelayConfig();

    // 创建互斥量，防止重复运行
    g_hMutex = CreateMutex(NULL, TRUE, TEXT("SOCDBridgeMutex"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, 
                   TEXT("SOCD_Bridge 已经在运行！"), 
                   TEXT("SOCD_Bridge"), 
                   MB_ICONINFORMATION | MB_OK);
        return 1;
    }

    // 注册窗口类
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("SOCDBridgeClass");

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, 
                   TEXT("窗口注册失败！"), 
                   TEXT("错误"), 
                   MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
        return 1;
    }

    // 创建主窗口（隐藏窗口，仅用于接收消息）
    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, TEXT("SOCD_Bridge"), 
                               WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
                               NULL, NULL, wc.hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, 
                   TEXT("窗口创建失败！"), 
                   TEXT("错误"), 
                   MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
        return 1;
    }

    // 初始化系统托盘图标
    InitNotifyIconData(hwnd);

    // 安装全局键盘钩子
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (g_hHook == NULL) {
        MessageBox(NULL, 
                   TEXT("键盘钩子安装失败！"), 
                   TEXT("错误"), 
                   MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
        return 1;
    }

    // 主消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 保存延迟配置
    SaveDelayConfig();

    // 清理资源
    UnhookWindowsHookEx(g_hHook);
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    ReleaseMutex(g_hMutex);
    CloseHandle(g_hMutex);

    return 0;
}

/* =========================================================================
 *                          函数实现 - 按键处理
 * ========================================================================= */

/**
 * @brief 处理按键按下事件
 * @param[in] keyCode 虚拟键码
 * @note 实现SOCD处理逻辑：
 *       - 如果是组内第一个按下或同一个键，直接发送
 *       - 如果是组内不同键，根据延迟设置进行切换
 */
void HandleKeyDown(int keyCode) {
    KeyState& currentKeyInfo = g_keyInfo[keyCode];
    GroupState& currentGroupInfo = g_groupInfo[currentKeyInfo.group];
    
    if (!currentKeyInfo.keyDown) {
        currentKeyInfo.keyDown = true;
        
        if (currentGroupInfo.activeKey == 0 || currentGroupInfo.activeKey == keyCode) {
            // 组内第一个按下或同一个键，直接发送
            currentGroupInfo.activeKey = keyCode;
            SendKey(keyCode, true);
        } else {
            // 组内不同键，需要切换
            currentGroupInfo.previousKey = currentGroupInfo.activeKey;
            currentGroupInfo.activeKey = keyCode;
            
            // 延迟模式：先保持同时输入，然后在随机延迟后切换
            if (g_delaySettings.enabled) {
                int delay = GetRandomDelay();
                Sleep(delay);
            }
            
            // 切换按键：释放旧键，按下新键
            SendKey(currentGroupInfo.previousKey, false);
            SendKey(keyCode, true);
        }
    }
}

/**
 * @brief 处理按键释放事件
 * @param[in] keyCode 虚拟键码
 * @note 处理按键释放时的状态恢复逻辑
 */
void HandleKeyUp(int keyCode) {
    KeyState& currentKeyInfo = g_keyInfo[keyCode];
    GroupState& currentGroupInfo = g_groupInfo[currentKeyInfo.group];
    
    // 清除无效的previousKey记录
    if (currentGroupInfo.previousKey == keyCode && !currentKeyInfo.keyDown) {
        currentGroupInfo.previousKey = 0;
    }
    
    if (currentKeyInfo.keyDown) {
        currentKeyInfo.keyDown = false;
        
        if (currentGroupInfo.activeKey == keyCode && currentGroupInfo.previousKey != 0) {
            // 当前激活键释放，恢复到上一个键
            SendKey(keyCode, false);
            currentGroupInfo.activeKey = currentGroupInfo.previousKey;
            currentGroupInfo.previousKey = 0;
            SendKey(currentGroupInfo.activeKey, true);
        } else {
            // 正常释放
            currentGroupInfo.previousKey = 0;
            if (currentGroupInfo.activeKey == keyCode) {
                currentGroupInfo.activeKey = 0;
            }
            SendKey(keyCode, false);
        }
    }
}

/**
 * @brief 检测是否为模拟按键事件
 * @param[in] flags 键盘事件标志
 * @return true表示是模拟按键，false表示是真实按键
 */
bool IsSimulatedKeyEvent(DWORD flags) {
    return (flags & 0x10) != 0;
}

/**
 * @brief 发送键盘输入事件
 * @param[in] targetKey 目标虚拟键码
 * @param[in] keyDown true表示按下，false表示释放
 * @note 使用SendInput API模拟键盘输入
 */
void SendKey(int targetKey, bool keyDown) {
    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = targetKey;
    input.ki.wScan = MapVirtualKey(targetKey, 0);
    
    DWORD flags = KEYEVENTF_SCANCODE;
    input.ki.dwFlags = keyDown ? flags : (flags | KEYEVENTF_KEYUP);
    
    SendInput(1, &input, sizeof(INPUT));
}

/**
 * @brief 获取随机延迟时间
 * @return 延迟时间(ms)，在设定延迟值上下浮动范围内随机
 * @note 如果未启用延迟或浮动范围为0，返回固定延迟值
 */
int GetRandomDelay() {
    if (!g_delaySettings.enabled || g_delaySettings.floatRangeMs <= 0) {
        return g_delaySettings.delayMs;
    }
    
    int minDelay = max(0, g_delaySettings.delayMs - g_delaySettings.floatRangeMs);
    int maxDelay = g_delaySettings.delayMs + g_delaySettings.floatRangeMs;
    
    return minDelay + (rand() % (maxDelay - minDelay + 1));
}

/* =========================================================================
 *                          函数实现 - 钩子回调
 * ========================================================================= */

/**
 * @brief 全局键盘钩子回调函数
 * @param[in] nCode 钩子代码
 * @param[in] wParam 消息类型（WM_KEYDOWN/WM_KEYUP等）
 * @param[in] lParam 按键信息结构体指针
 * @return LRESULT 钩子链返回值
 * @note 拦截已注册的按键，调用相应的处理函数
 */
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!g_isLocked && nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyBoard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        
        // 忽略模拟按键事件，防止递归
        if (!IsSimulatedKeyEvent(pKeyBoard->flags)) {
            if (g_keyInfo[pKeyBoard->vkCode].registered) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    HandleKeyDown(pKeyBoard->vkCode);
                }
                if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                    HandleKeyUp(pKeyBoard->vkCode);
                }
                return 1;  // 阻止按键传递
            }
        }
    }
    
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

/* =========================================================================
 *                          函数实现 - 系统托盘
 * ========================================================================= */

/**
 * @brief 初始化系统托盘图标数据
 * @param[in] hwnd 关联的窗口句柄
 * @note 设置托盘图标、提示文本和回调消息
 */
void InitNotifyIconData(HWND hwnd) {
    memset(&g_nid, 0, sizeof(NOTIFYICONDATA));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAY_APP_ICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    // 尝试加载自定义图标，失败则使用系统默认图标
    HICON hIcon = static_cast<HICON>(
        LoadImage(NULL, TEXT("icon.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE)
    );
    g_nid.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    
    lstrcpy(g_nid.szTip, TEXT("SOCD_Bridge"));
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

/* =========================================================================
 *                          函数实现 - 主窗口过程
 * ========================================================================= */

/**
 * @brief 主窗口消息处理函数
 * @param[in] hwnd 窗口句柄
 * @param[in] msg 消息类型
 * @param[in] wParam 消息参数
 * @param[in] lParam 消息参数
 * @return LRESULT 消息处理结果
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    /* -----------------------------------------------------------------
     *                      托盘图标消息处理
     * ----------------------------------------------------------------- */
    case WM_TRAYICON:
        if (lParam == static_cast<LPARAM>(WM_RBUTTONDOWN)) {
            // 右键点击：显示上下文菜单
            POINT curPoint;
            GetCursorPos(&curPoint);
            SetForegroundWindow(hwnd);

            HMENU hMenu = CreatePopupMenu();
            
            // 重新绑定按键菜单项
            AppendMenu(hMenu, MF_STRING, ID_TRAY_REBIND_KEYS, TEXT("重新绑定按键"));

            // 布局子菜单
            HMENU hSubMenu = CreatePopupMenu();
            vector<string> layouts = ListLayouts();
            if (!layouts.empty()) {
                for (size_t i = 0; i < layouts.size(); ++i) {
                    AppendMenuA(hSubMenu, MF_STRING, 
                               static_cast<UINT>(ID_LAYOUT_BASE + i), 
                               layouts[i].c_str());
                }
            } else {
                AppendMenu(hSubMenu, MF_GRAYED, 0, TEXT("未找到布局"));
            }
            AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenu), 
                       TEXT("选择配置方案"));

            // 功能菜单项
            AppendMenu(hMenu, MF_STRING, ID_TRAY_RESTART_SOCD_BRIDGE, 
                       TEXT("重启 SOCD_Bridge"));
            AppendMenu(hMenu, MF_STRING, ID_TRAY_LOCK_FUNCTION, 
                       g_isLocked ? TEXT("启用 SOCD_Bridge") : TEXT("禁用 SOCD_Bridge"));
            
            // 延迟设置菜单项（显示当前状态）
            {
                char delayMenuText[64];
                if (g_delaySettings.enabled) {
                    sprintf_s(delayMenuText, sizeof(delayMenuText),
                             "输入延迟设置 [已启用 %dms]", g_delaySettings.delayMs);
                } else {
                    sprintf_s(delayMenuText, sizeof(delayMenuText),
                             "输入延迟设置 [已禁用]");
                }
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_DELAY_SETTINGS, delayMenuText);
            }
            
            // 分隔线和帮助菜单项
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_HELP, TEXT("获取帮助"));
            AppendMenu(hMenu, MF_STRING, ID_TRAY_VERSION_INFO, TEXT("版本信息 (1.1)"));
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT_CONTEXT_MENU_ITEM, 
                       TEXT("退出 SOCD_Bridge"));

            // 显示菜单
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, 
                          curPoint.x, curPoint.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        else if (lParam == static_cast<LPARAM>(WM_LBUTTONDBLCLK)) {
            // 双击左键：切换锁定状态
            g_isLocked = !g_isLocked;
            HICON hIcon = g_isLocked
                ? static_cast<HICON>(LoadImage(NULL, TEXT("icon_off.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE))
                : static_cast<HICON>(LoadImage(NULL, TEXT("icon.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
            if (hIcon) {
                g_nid.hIcon = hIcon;
                Shell_NotifyIcon(NIM_MODIFY, &g_nid);
                DestroyIcon(hIcon);
            }
        }
        break;

    /* -----------------------------------------------------------------
     *                      命令消息处理
     * ----------------------------------------------------------------- */
    case WM_COMMAND:
        if (LOWORD(wParam) >= ID_LAYOUT_BASE) {
            // 布局选择命令
            int layoutIndex = LOWORD(wParam) - ID_LAYOUT_BASE;
            vector<string> layouts = ListLayouts();
            if (layoutIndex >= 0 && layoutIndex < static_cast<int>(layouts.size())) {
                ApplyLayout(layouts[layoutIndex]);
                RestartSOCDBridge();
            }
        }
        else {
            // 其他命令
            switch (LOWORD(wParam)) {
            case ID_TRAY_EXIT_CONTEXT_MENU_ITEM: {
                int result = MessageBoxA(hwnd, "确定要退出 SOCD_Bridge 吗？", 
                                        "SOCD_Bridge", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
                if (result == IDOK) {
                    PostQuitMessage(0);
                }
                break;
            }
                
            case ID_TRAY_VERSION_INFO: {
                ShowVersionDialog(hwnd);
                break;
            }
                
            case ID_TRAY_REBIND_KEYS:
                ShellExecute(NULL, TEXT("open"), TEXT("config.cfg"), 
                           NULL, NULL, SW_SHOWNORMAL);
                break;
                
            case ID_TRAY_HELP:
                ShellExecute(NULL, TEXT("open"), 
                           TEXT("https://github.com/Bichuntea/SOCD_Bridge"), 
                           NULL, NULL, SW_SHOWNORMAL);
                break;
                
            case ID_TRAY_RESTART_SOCD_BRIDGE:
                RestartSOCDBridge();
                break;
                
            case ID_TRAY_LOCK_FUNCTION:
                g_isLocked = !g_isLocked;
                {
                    HICON hIcon = g_isLocked
                        ? static_cast<HICON>(LoadImage(NULL, TEXT("icon_off.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE))
                        : static_cast<HICON>(LoadImage(NULL, TEXT("icon.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
                    if (hIcon) {
                        g_nid.hIcon = hIcon;
                        Shell_NotifyIcon(NIM_MODIFY, &g_nid);
                        DestroyIcon(hIcon);
                    }
                }
                break;
                
            case ID_TRAY_DELAY_SETTINGS:
                ShowDelaySettingsDialog(hwnd);
                break;
            }
        }
        break;

    /* -----------------------------------------------------------------
     *                      窗口销毁处理
     * ----------------------------------------------------------------- */
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

/* =========================================================================
 *                          函数实现 - 配置管理
 * ========================================================================= */

/**
 * @brief 延迟配置文件路径
 */
static const char* DELAY_CONFIG_FILE = "delay.cfg";

/**
 * @brief 加载延迟配置文件
 * @note 从 delay.cfg 文件中读取延迟设置参数
 * @details 配置文件格式：
 *          enabled=0 或 1
 *          delay=40
 *          float_range=20
 */
void LoadDelayConfig() {
    ifstream configFile(DELAY_CONFIG_FILE);
    if (!configFile.is_open()) {
        // 配置文件不存在，使用默认值
        return;
    }

    string line;
    while (getline(configFile, line)) {
        istringstream iss(line);
        string key;
        int value;

        if (getline(iss, key, '=') && (iss >> value)) {
            if (key == "enabled") {
                g_delaySettings.enabled = (value != 0);
            } else if (key == "delay") {
                g_delaySettings.delayMs = value;
                if (g_delaySettings.delayMs < MIN_DELAY_MS) {
                    g_delaySettings.delayMs = MIN_DELAY_MS;
                }
                if (g_delaySettings.delayMs > MAX_DELAY_MS) {
                    g_delaySettings.delayMs = MAX_DELAY_MS;
                }
            } else if (key == "float_range") {
                g_delaySettings.floatRangeMs = value;
                if (g_delaySettings.floatRangeMs < MIN_FLOAT_RANGE_MS) {
                    g_delaySettings.floatRangeMs = MIN_FLOAT_RANGE_MS;
                }
                if (g_delaySettings.floatRangeMs > MAX_FLOAT_RANGE_MS) {
                    g_delaySettings.floatRangeMs = MAX_FLOAT_RANGE_MS;
                }
            }
        }
    }
}

/**
 * @brief 保存延迟配置文件
 * @note 将当前延迟设置参数保存到 delay.cfg 文件
 */
void SaveDelayConfig() {
    ofstream configFile(DELAY_CONFIG_FILE);
    if (!configFile.is_open()) {
        return;
    }

    configFile << "enabled=" << (g_delaySettings.enabled ? 1 : 0) << endl;
    configFile << "delay=" << g_delaySettings.delayMs << endl;
    configFile << "float_range=" << g_delaySettings.floatRangeMs << endl;
}

/**
 * @brief 获取版本信息字符串
 * @return 版本信息字符串
 */
string GetVersionInfo() {
    return "SOCD_Bridge v1.1\n\n"
           "许可证：MIT 许可证\n";
}

/**
 * @brief 在浏览器中打开GitHub页面
 * @note 使用ShellExecute打开默认浏览器访问项目GitHub页面
 */
void OpenGitHubPage() {
    ShellExecuteW(NULL, L"open", L"https://github.com/Bichuntea/SOCD_Bridge", NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief 版本信息对话框消息处理函数
 * @param[in] hwnd 对话框窗口句柄
 * @param[in] msg 消息类型
 * @param[in] wParam 消息参数
 * @param[in] lParam 消息参数
 * @return LRESULT 消息处理结果
 */
LRESULT CALLBACK VersionDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFont = NULL;
    
    switch (msg) {
    case WM_CREATE: {
        // 创建字体 - 使用系统默认字体
        NONCLIENTMETRICS ncm;
        memset(&ncm, 0, sizeof(NONCLIENTMETRICS));
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
        hFont = CreateFontIndirect(&ncm.lfMessageFont);
        
        // 版本信息标题 - 左对齐
        HWND hLabelTitle = CreateWindow(TEXT("STATIC"), TEXT("SOCD_Bridge v1.1"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 300, 25, hwnd, NULL, NULL, NULL);
        SendMessage(hLabelTitle, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // SysLink 控件 - 可点击的GitHub链接
        HWND hSysLink = CreateWindowExW(0, WC_LINK, 
            L"Github：<a href=\"https://github.com/Bichuntea/SOCD_Bridge\">https://github.com/Bichuntea/SOCD_Bridge</a>",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            20, 55, 340, 25, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_VERSION_SYSLINK)), 
            NULL, NULL);
        SendMessage(hSysLink, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 许可证信息
        HWND hLabelLicense = CreateWindow(TEXT("STATIC"), TEXT("许可证：MIT 许可证"),
            WS_CHILD | WS_VISIBLE,
            20, 90, 300, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hLabelLicense, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 确定按钮 - 左对齐
        HWND hButtonOk = CreateWindow(TEXT("BUTTON"), TEXT("确定"),
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            20, 125, 80, 30, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_VERSION_BUTTON_OK)),
            NULL, NULL);
        SendMessage(hButtonOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        break;
    }
    
    case WM_CTLCOLORDLG:
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdcStatic, TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    
    case WM_NOTIFY: {
        NMLINK* pNMLink = reinterpret_cast<NMLINK*>(lParam);
        if (pNMLink->hdr.idFrom == ID_VERSION_SYSLINK && pNMLink->hdr.code == NM_CLICK) {
            // 点击链接 - 打开浏览器
            OpenGitHubPage();
            return TRUE;
        }
        break;
    }
    
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_VERSION_BUTTON_OK) {
            DestroyWindow(hwnd);
        }
        break;
    }
    
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
        
    case WM_DESTROY:
        if (hFont) {
            DeleteObject(hFont);
            hFont = NULL;
        }
        break;
        
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

/**
 * @brief 显示版本信息对话框
 * @param[in] hwndParent 父窗口句柄
 */
void ShowVersionDialog(HWND hwndParent) {
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = VersionDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("VersionInfoDialog");
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassEx(&wc);
    
    // 计算屏幕中心位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int dialogWidth = 380;
    int dialogHeight = 210;
    int x = (screenWidth - dialogWidth) / 2;
    int y = (screenHeight - dialogHeight) / 2;
    
    HWND hDialog = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        TEXT("VersionInfoDialog"),
        TEXT("SOCD_Bridge 版本信息"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, dialogWidth, dialogHeight,
        hwndParent, NULL, GetModuleHandle(NULL), NULL);
    
    if (hDialog) {
        ShowWindow(hDialog, SW_SHOW);
        UpdateWindow(hDialog);
        
        // 对话框消息循环
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            if (!IsDialogMessage(hDialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
}

/**
 * @brief 从备份恢复配置文件
 * @param[in] backupFilename 备份文件名
 * @param[in] destinationFilename 目标文件名
 * @note 从 meta 目录读取备份文件并复制到目标位置
 */
void RestoreConfigFromBackup(const string& backupFilename, const string& destinationFilename) {
    const string sourcePath = "meta\\" + backupFilename;
    const string destinationPath = destinationFilename;

    if (CopyFileA(sourcePath.c_str(), destinationPath.c_str(), FALSE)) {
        MessageBox(NULL, 
                   TEXT("已从备份成功恢复默认配置。"), 
                   TEXT("SOCD_Bridge"), 
                   MB_ICONINFORMATION | MB_OK);
    } else {
        MessageBox(NULL, 
                   TEXT("从备份恢复配置失败。"), 
                   TEXT("SOCD_Bridge 错误"), 
                   MB_ICONERROR | MB_OK);
    }
}

/**
 * @brief 创建默认配置文件
 * @param[in] filename 配置文件名
 * @note 从备份文件恢复默认配置
 */
void CreateDefaultConfig(const string& filename) {
    RestoreConfigFromBackup("backup.socd_bridge", filename);
}

/**
 * @brief 加载配置文件
 * @param[in] filename 配置文件路径
 * @return true表示加载成功，false表示加载失败
 * @note 解析配置文件，注册按键分组信息
 * @details 配置文件格式：
 *          [Group]
 *          key1=<虚拟键码>
 *          key2=<虚拟键码>
 */
bool LoadConfig(const string& filename) {
    ifstream configFile(filename);
    if (!configFile.is_open()) {
        CreateDefaultConfig(filename);
        return false;
    }

    string line;
    int groupId = 0;
    const regex sectionPattern(R"(\s*\[Group\]\s*)");
    
    while (getline(configFile, line)) {
        istringstream iss(line);
        string key;
        int value;
        
        if (regex_match(line, sectionPattern)) {
            // 遇到 [Group] 标记，增加组ID
            groupId++;
        } else if (getline(iss, key, '=') && (iss >> value)) {
            // 解析 key=<value> 格式
            if (key.find("key") != string::npos) {
                if (!g_keyInfo[value].registered) {
                    g_keyInfo[value].registered = true;
                    g_keyInfo[value].group = groupId;
                } else {
                    // 检测到重复按键配置
                    MessageBox(NULL,
                               TEXT("配置文件中包含重复的按键，请检查配置。"),
                               TEXT("SOCD_Bridge 错误"), 
                               MB_ICONEXCLAMATION | MB_OK);
                    return false;
                }
            }
        }
    }
    
    return true;
}

/* =========================================================================
 *                          函数实现 - 工具函数
 * ========================================================================= */

/* =========================================================================
 *                          函数实现 - 延迟设置对话框
 * ========================================================================= */

/**
 * @brief 延迟设置对话框消息处理函数
 * @param[in] hwnd 对话框窗口句柄
 * @param[in] msg 消息类型
 * @param[in] wParam 消息参数
 * @param[in] lParam 消息参数
 * @return LRESULT 消息处理结果
 */
LRESULT CALLBACK DelayDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 静态变量，用于存储对话框控件句柄
    static HWND hCheckboxEnable = NULL;     // 启用延迟复选框
    static HWND hSliderDelay = NULL;        // 延迟时间滑块
    static HWND hEditDelay = NULL;          // 延迟时间输入框
    static HWND hSliderFloat = NULL;        // 浮动范围滑块
    static HWND hEditFloat = NULL;          // 浮动范围输入框
    static HWND hLabelDelayValue = NULL;    // 延迟时间显示标签
    static HWND hLabelFloatValue = NULL;    // 浮动范围显示标签
    static HWND hButtonCancel = NULL;       // 取消按钮
    static HFONT hFont = NULL;              // 字体句柄
    
    switch (msg) {
    /* -----------------------------------------------------------------
     *                      对话框创建消息
     * ----------------------------------------------------------------- */
    case WM_CREATE: {
        // 创建字体 - 使用系统默认字体，自动适应系统字体大小
        NONCLIENTMETRICS ncm;
        memset(&ncm, 0, sizeof(NONCLIENTMETRICS));
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
        hFont = CreateFontIndirect(&ncm.lfMessageFont);
        
        // 启用复选框 - 放在顶部
        hCheckboxEnable = CreateWindow(TEXT("BUTTON"), TEXT("启用输入延迟"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 20, 200, 24, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_CHECKBOX_ENABLE)), 
            NULL, NULL);
        SendMessage(hCheckboxEnable, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        if (g_delaySettings.enabled) {
            SendMessage(hCheckboxEnable, BM_SETCHECK, BST_CHECKED, 0);
        }
        
        /* === 延迟时间设置区域 === */
        HWND hLabelDelay = CreateWindow(TEXT("STATIC"), TEXT("延迟时间 (ms):"),
            WS_CHILD | WS_VISIBLE,
            20, 60, 120, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hLabelDelay, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 延迟时间滑块
        hSliderDelay = CreateWindow(TRACKBAR_CLASS, TEXT(""),
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            20, 85, 200, 30, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_SLIDER_DELAY)), 
            NULL, NULL);
        SendMessage(hSliderDelay, TBM_SETRANGE, TRUE, MAKELONG(MIN_DELAY_MS, MAX_DELAY_MS));
        SendMessage(hSliderDelay, TBM_SETPOS, TRUE, g_delaySettings.delayMs);
        SendMessage(hSliderDelay, TBM_SETTICFREQ, 20, 0);
        
        // 延迟时间输入框
        char delayStr[16];
        sprintf_s(delayStr, sizeof(delayStr), "%d", g_delaySettings.delayMs);
        hEditDelay = CreateWindow(TEXT("EDIT"), TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
            230, 85, 50, 24, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_EDIT_DELAY)),
            NULL, NULL);
        SendMessage(hEditDelay, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SetWindowTextA(hEditDelay, delayStr);

        // 延迟时间单位显示
        hLabelDelayValue = CreateWindow(TEXT("STATIC"), TEXT("ms"),
            WS_CHILD | WS_VISIBLE,
            285, 88, 30, 20, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_LABEL_DELAY_VALUE)),
            NULL, NULL);
        SendMessage(hLabelDelayValue, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 延迟时间确定按钮
        HWND hBtnConfirmDelay = CreateWindow(TEXT("BUTTON"), TEXT("确定"),
            WS_CHILD | WS_VISIBLE,
            340, 85, 40, 24, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_BUTTON_CONFIRM_DELAY)), 
            NULL, NULL);
        SendMessage(hBtnConfirmDelay, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        /* === 浮动范围设置区域 === */
        HWND hLabelFloat = CreateWindow(TEXT("STATIC"), TEXT("浮动范围 (ms):"),
            WS_CHILD | WS_VISIBLE,
            20, 125, 120, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hLabelFloat, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 浮动范围滑块
        hSliderFloat = CreateWindow(TRACKBAR_CLASS, TEXT(""),
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            20, 150, 200, 30, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_SLIDER_FLOAT)), 
            NULL, NULL);
        SendMessage(hSliderFloat, TBM_SETRANGE, TRUE, MAKELONG(MIN_FLOAT_RANGE_MS, MAX_FLOAT_RANGE_MS));
        SendMessage(hSliderFloat, TBM_SETPOS, TRUE, g_delaySettings.floatRangeMs);
        SendMessage(hSliderFloat, TBM_SETTICFREQ, 5, 0);
        
        // 浮动范围输入框
        char floatStr[16];
        sprintf_s(floatStr, sizeof(floatStr), "%d", g_delaySettings.floatRangeMs);
        hEditFloat = CreateWindow(TEXT("EDIT"), TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
            230, 150, 50, 24, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_EDIT_FLOAT)),
            NULL, NULL);
        SendMessage(hEditFloat, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SetWindowTextA(hEditFloat, floatStr);

        // 浮动范围单位显示
        hLabelFloatValue = CreateWindow(TEXT("STATIC"), TEXT("ms"),
            WS_CHILD | WS_VISIBLE,
            285, 153, 30, 20, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_LABEL_FLOAT_VALUE)),
            NULL, NULL);
        SendMessage(hLabelFloatValue, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 浮动范围确定按钮
        HWND hBtnConfirmFloat = CreateWindow(TEXT("BUTTON"), TEXT("确定"),
            WS_CHILD | WS_VISIBLE,
            340, 150, 40, 24, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_BUTTON_CONFIRM_FLOAT)), 
            NULL, NULL);
        SendMessage(hBtnConfirmFloat, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        /* === 底部按钮区域 === */
        // 分隔线
        CreateWindow(TEXT("STATIC"), TEXT(""),
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            20, 190, 390, 2, hwnd, NULL, NULL, NULL);
        
        // 恢复默认按钮
        HWND hButtonReset = CreateWindow(TEXT("BUTTON"), TEXT("恢复默认"),
            WS_CHILD | WS_VISIBLE,
            100, 205, 80, 30, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_BUTTON_RESET)), 
            NULL, NULL);
        SendMessage(hButtonReset, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 应用按钮（主按钮）
        HWND hButtonApply = CreateWindow(TEXT("BUTTON"), TEXT("应用"),
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            200, 205, 80, 30, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_BUTTON_APPLY)), 
            NULL, NULL);
        SendMessage(hButtonApply, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        // 取消按钮
        hButtonCancel = CreateWindow(TEXT("BUTTON"), TEXT("取消"),
            WS_CHILD | WS_VISIBLE,
            300, 205, 80, 30, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DELAY_BUTTON_CANCEL)), 
            NULL, NULL);
        SendMessage(hButtonCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        
        break;
    }
    
    /* -----------------------------------------------------------------
     *                      控件颜色消息处理
     * ----------------------------------------------------------------- */
    case WM_CTLCOLORDLG:
        // 对话框背景颜色
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        
    case WM_CTLCOLORSTATIC: {
        // 静态控件背景颜色
        HDC hdcStatic = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdcStatic, TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    
    /* -----------------------------------------------------------------
     *                      滚动条消息处理
     * ----------------------------------------------------------------- */
    case WM_HSCROLL: {
        HWND hSlider = reinterpret_cast<HWND>(lParam);
        
        if (hSlider == hSliderDelay) {
            // 延迟时间滑块变化
            int pos = static_cast<int>(SendMessage(hSliderDelay, TBM_GETPOS, 0, 0));
            g_delaySettings.delayMs = pos;
            
            char posStr[16];
            sprintf_s(posStr, sizeof(posStr), "%d", pos);
            
            SetWindowTextA(hEditDelay, posStr);
        } 
        else if (hSlider == hSliderFloat) {
            // 浮动范围滑块变化
            int pos = static_cast<int>(SendMessage(hSliderFloat, TBM_GETPOS, 0, 0));
            g_delaySettings.floatRangeMs = pos;
            
            char posStr[16];
            sprintf_s(posStr, sizeof(posStr), "%d", pos);
            
            SetWindowTextA(hEditFloat, posStr);
        }
        break;
    }
    
    /* -----------------------------------------------------------------
     *                      命令消息处理
     * ----------------------------------------------------------------- */
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        
        if (id == ID_DELAY_BUTTON_APPLY) {
            /* 应用按钮 - 保存所有设置并关闭 */
            char buffer[32];
            
            // 读取延迟时间
            GetWindowTextA(hEditDelay, buffer, sizeof(buffer));
            g_delaySettings.delayMs = atoi(buffer);
            if (g_delaySettings.delayMs < MIN_DELAY_MS) {
                g_delaySettings.delayMs = MIN_DELAY_MS;
            }
            
            // 读取浮动范围
            GetWindowTextA(hEditFloat, buffer, sizeof(buffer));
            g_delaySettings.floatRangeMs = atoi(buffer);
            if (g_delaySettings.floatRangeMs < MIN_FLOAT_RANGE_MS) {
                g_delaySettings.floatRangeMs = MIN_FLOAT_RANGE_MS;
            }
            
            // 读取启用状态
            g_delaySettings.enabled = (SendMessage(hCheckboxEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
            
            // 保存配置到文件
            SaveDelayConfig();
            
            DestroyWindow(hwnd);
            g_hDelayDialog = NULL;
        } 
        else if (id == ID_DELAY_BUTTON_RESET) {
            /* 恢复默认按钮 - 恢复默认设置 */
            g_delaySettings.delayMs = DEFAULT_DELAY_MS;
            g_delaySettings.floatRangeMs = DEFAULT_FLOAT_RANGE_MS;
            
            // 更新滑块
            SendMessage(hSliderDelay, TBM_SETPOS, TRUE, g_delaySettings.delayMs);
            SendMessage(hSliderFloat, TBM_SETPOS, TRUE, g_delaySettings.floatRangeMs);
            
            // 更新输入框
            char delayStr[16], floatStr[16];
            sprintf_s(delayStr, sizeof(delayStr), "%d", g_delaySettings.delayMs);
            sprintf_s(floatStr, sizeof(floatStr), "%d", g_delaySettings.floatRangeMs);
            SetWindowTextA(hEditDelay, delayStr);
            SetWindowTextA(hEditFloat, floatStr);
        } 
        else if (id == ID_DELAY_BUTTON_CONFIRM_DELAY) {
            /* 延迟时间确定按钮 - 仅确认延迟时间 */
            char buffer[32];
            GetWindowTextA(hEditDelay, buffer, sizeof(buffer));
            g_delaySettings.delayMs = atoi(buffer);
            if (g_delaySettings.delayMs < MIN_DELAY_MS) {
                g_delaySettings.delayMs = MIN_DELAY_MS;
            }
            // 更新滑块位置
            SendMessage(hSliderDelay, TBM_SETPOS, TRUE, g_delaySettings.delayMs);
        } 
        else if (id == ID_DELAY_BUTTON_CONFIRM_FLOAT) {
            /* 浮动范围确定按钮 - 仅确认浮动范围 */
            char buffer[32];
            GetWindowTextA(hEditFloat, buffer, sizeof(buffer));
            g_delaySettings.floatRangeMs = atoi(buffer);
            if (g_delaySettings.floatRangeMs < MIN_FLOAT_RANGE_MS) {
                g_delaySettings.floatRangeMs = MIN_FLOAT_RANGE_MS;
            }
            // 更新滑块位置
            SendMessage(hSliderFloat, TBM_SETPOS, TRUE, g_delaySettings.floatRangeMs);
        } 
        else if (id == ID_DELAY_BUTTON_CANCEL) {
            /* 取消按钮 - 关闭对话框 */
            DestroyWindow(hwnd);
            g_hDelayDialog = NULL;
        } 
        else if (id == ID_DELAY_EDIT_DELAY && code == EN_CHANGE) {
            /* 延迟时间输入框内容变化 */
            char buffer[32];
            GetWindowTextA(hEditDelay, buffer, sizeof(buffer));
            int val = atoi(buffer);
            if (val >= MIN_DELAY_MS && val <= MAX_DELAY_MS) {
                SendMessage(hSliderDelay, TBM_SETPOS, TRUE, val);
            }
        } 
        else if (id == ID_DELAY_EDIT_FLOAT && code == EN_CHANGE) {
            /* 浮动范围输入框内容变化 */
            char buffer[32];
            GetWindowTextA(hEditFloat, buffer, sizeof(buffer));
            int val = atoi(buffer);
            if (val >= MIN_FLOAT_RANGE_MS && val <= MAX_FLOAT_RANGE_MS) {
                SendMessage(hSliderFloat, TBM_SETPOS, TRUE, val);
            }
        }
        break;
    }
    
    /* -----------------------------------------------------------------
     *                      窗口关闭/销毁消息
     * ----------------------------------------------------------------- */
    case WM_CLOSE:
        DestroyWindow(hwnd);
        g_hDelayDialog = NULL;
        break;
        
    case WM_DESTROY:
        // 清理字体资源
        if (hFont) {
            DeleteObject(hFont);
            hFont = NULL;
        }
        break;
        
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

/**
 * @brief 显示延迟设置对话框
 * @param[in] hwndParent 父窗口句柄
 * @note 如果对话框已存在，则将其置于前台
 */
void ShowDelaySettingsDialog(HWND hwndParent) {
    // 如果对话框已存在，将其置于前台
    if (g_hDelayDialog != NULL) {
        SetForegroundWindow(g_hDelayDialog);
        return;
    }
    
    // 注册对话框窗口类
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DelayDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("DelaySettingsDialog");
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassEx(&wc);
    
    // 计算屏幕中心位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - DIALOG_WIDTH) / 2;
    int y = (screenHeight - DIALOG_HEIGHT) / 2;
    
    // 创建对话框窗口
    g_hDelayDialog = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        TEXT("DelaySettingsDialog"),
        TEXT("输入延迟设置"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, DIALOG_WIDTH, DIALOG_HEIGHT,
        hwndParent, NULL, GetModuleHandle(NULL), NULL);
    
    if (g_hDelayDialog) {
        ShowWindow(g_hDelayDialog, SW_SHOW);
        UpdateWindow(g_hDelayDialog);
        
        // 对话框消息循环
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            if (!IsDialogMessage(g_hDelayDialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (g_hDelayDialog == NULL) {
                break;
            }
        }
    }
}

/* =========================================================================
 *                              文件结束
 * ========================================================================= */
