#include <iostream>
#include <string>
#include <locale>
#include <codecvt>

#include "resource.h"
#include "ChatClient.h"
#include <windows.h>
#include <commctrl.h>


#pragma comment(lib, "comctl32.lib")


INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
void CALLBACK MonitorNetworkStatusTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

void LoginMessageCallback(const std::string& message, int status);
void MainMessageCallback(int licenseid,  int type, string& msg);

void OnLogin(HWND hDlg);

void InitializeListView(HWND hWnd);
void AddListViewItems(HWND hWnd);
void RunEngine(HWND hWnd, int itemIndex);

std::string WCharToString(const WCHAR* wstr);
std::wstring StringToWString(const std::string& str);
std::wstring TimePointToWString(const std::chrono::system_clock::time_point& timePoint);

HINSTANCE hinst;
HWND mainHwnd, listView;

evpp::EventLoopThread gLoop;
ChatClient * gClient = NULL;


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    gLoop.Start(true);

    hinst = hInstance;
    DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_LOGIN), nullptr, MainDlgProc);

    gLoop.Stop(true);

    return 0;
}

INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG: {

        SetDlgItemTextA(hDlg, IDC_EDIT_SERVER, ("127.0.0.1"));
        SetDlgItemTextA(hDlg, IDC_EDIT_PORT, ("9099"));
        SetDlgItemTextA(hDlg, IDC_EDIT_ID, ("victor"));
        SetDlgItemTextA(hDlg, IDC_EDIT_PASSWORD, ("abc123"));

        mainHwnd = hDlg;
        listView = CreateWindowW(WC_LISTVIEW, nullptr,
            WS_CHILD | WS_VISIBLE | LVS_REPORT,
            50, 120, 1000, 500,
            hDlg, nullptr, (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE), nullptr);
        InitializeListView(listView);

        UINT_PTR g_timerId = SetTimer(hDlg, 1, 1000, MonitorNetworkStatusTimerProc);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CONNECT) {
            OnLogin(hDlg);
        }
        if (LOWORD(wParam) == IDC_LOGIN_EXIT) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;

    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == listView && ((LPNMHDR)lParam)->code == NM_DBLCLK) {
            LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
            if (lpnmitem->iItem != -1) {
                RunEngine(listView, lpnmitem->iItem);
            }
        }
        break;

    case WM_DESTROY:
        EndDialog(hDlg, 0);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hDlg, message, wParam, lParam);
    }

    return (INT_PTR)FALSE;
}

void CALLBACK MonitorNetworkStatusTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (gClient) {
        int status = gClient->GetNetworkStatus();
        /*kDisconnected = 0,
        kConnecting = 1,
        kConnected = 2,
        kDisconnecting = 3,*/

        switch (status) {
        case 0:
            SetDlgItemText(mainHwnd, IDC_LOGIN_STATUS, TEXT("Disconnected"));
            break;
        case 1:
            SetDlgItemText(mainHwnd, IDC_LOGIN_STATUS, TEXT("Connecting to server ..."));
            break;
        case 2:
            SetDlgItemText(mainHwnd, IDC_LOGIN_STATUS, TEXT("Connected"));
            break;
        case 3:
            SetDlgItemText(mainHwnd, IDC_LOGIN_STATUS, TEXT("Disconnecting"));
            break;
        default:
            break;
        }
    }
}

void LoginMessageCallback(const std::string& message, int status) {
    
    SetDlgItemTextA(mainHwnd, IDC_LOGIN_STATUS, message.c_str());
    if (status == RESPONSE_RETURN_SUC) {
        AddListViewItems(listView);
    }
    else {
        ListView_DeleteAllItems(listView);
    }
}

void MainMessageCallback(int licenseid, int type, string& msg)
{
    if (type == 0) { // update use state
        ListView_DeleteAllItems(listView);
        AddListViewItems(listView);
    }
}


std::string WCharToString(const WCHAR* wstr) {
    if (wstr == nullptr) {
        return "";
    }

    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        DWORD error = GetLastError();
        std::cerr << "WideCharToMultiByte failed with error: " << error << std::endl;
        return "";
    }

    std::string str(bufferSize, 0);
    int convertedChars = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], bufferSize, nullptr, nullptr);
    if (convertedChars == 0) {
        DWORD error = GetLastError();
        std::cerr << "WideCharToMultiByte failed with error: " << error << std::endl;
        return "";
    }

    // Remove the null-terminator added by WideCharToMultiByte
    str.resize(bufferSize - 1);

    return str;
}

std::wstring StringToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::wstring TimePointToWString(const std::chrono::system_clock::time_point& timePoint) {
    std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* localTime = std::localtime(&time);

    std::wostringstream woss;
    const size_t bufferSize = 64;
    std::unique_ptr<wchar_t[]> buffer(new wchar_t[bufferSize]);
    if (std::wcsftime(buffer.get(), bufferSize, L"%Y-%m-%d %H:%M:%S", localTime) > 0) {
        woss << buffer.get();
    }
    return woss.str();
}

std::wstring convertToWString(const std::string& input) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(input);
}


void OnLogin(HWND hDlg) {
    WCHAR id[100], password[100], server[100], port[100];

    GetDlgItemText(hDlg, IDC_EDIT_SERVER, server, 100);
    GetDlgItemText(hDlg, IDC_EDIT_PORT, port, 100);
    GetDlgItemText(hDlg, IDC_EDIT_ID, id, 100);
    GetDlgItemText(hDlg, IDC_EDIT_PASSWORD, password, 100);

    // Convert WCHAR arrays to std::string
    std::string serverStr = WCharToString(server);
    std::string portStr = WCharToString(port);
    std::string idStr = WCharToString(id);
    std::string passwordStr = WCharToString(password);

    // Construct the host string
    std::string hostStr = serverStr + ":" + portStr;

    ListView_DeleteAllItems(listView);

    if (gClient == NULL) {
        gClient = new ChatClient(gLoop.loop(), hostStr);
        gClient->SetLoginMessageCallback(LoginMessageCallback);
        gClient->SetMainMessageCallback(MainMessageCallback);
        gClient->Connect(idStr, passwordStr);
    } else {
        gClient->Connect(idStr, passwordStr);
    }

    SetDlgItemTextA(hDlg, IDC_LOGIN_STATUS, "Connecting ...");
    HWND button = GetDlgItem(hDlg, IDC_CONNECT);;
    EnableWindow(button, false);
}

void InitializeListView(HWND hWnd) {
    LVCOLUMN lvColumn;

    lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvColumn.pszText = (LPWSTR)TEXT("LicenseID");
    lvColumn.cx = 50;
    lvColumn.iSubItem = 0;
    ListView_InsertColumn(hWnd, 0, &lvColumn);

    lvColumn.pszText = (LPWSTR)TEXT("ProductName");
    lvColumn.cx = 150;
    lvColumn.iSubItem = 1;
    ListView_InsertColumn(hWnd, 1, &lvColumn);

    lvColumn.pszText = (LPWSTR)TEXT("Type");
    lvColumn.cx = 100;
    lvColumn.iSubItem = 2;
    ListView_InsertColumn(hWnd, 2, &lvColumn);

    lvColumn.pszText = (LPWSTR)TEXT("AcessToken");
    lvColumn.cx = 400;
    lvColumn.iSubItem = 3;
    ListView_InsertColumn(hWnd, 3, &lvColumn);

    lvColumn.pszText = (LPWSTR)TEXT("Access Time");
    lvColumn.cx = 200;
    lvColumn.iSubItem = 4;
    ListView_InsertColumn(hWnd, 4, &lvColumn);

    lvColumn.pszText = (LPWSTR)TEXT("InUse");
    lvColumn.cx = 100;
    lvColumn.iSubItem = 5;
    ListView_InsertColumn(hWnd, 5, &lvColumn);
}

// Add items to the ListView control
void AddListViewItems(HWND hWnd) {

    LVITEM lvItem;

    vector <LicenseInstance> list = gClient->GetLicenseList();

    for (int i = 0; i < list.size(); ++i) {
        LicenseInstance info = list.at(i);

        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = i;

        lvItem.iSubItem = 0;
        std::wstring idStr = std::to_wstring(info.id);
        lvItem.pszText = const_cast<LPWSTR>(idStr.c_str());
        ListView_InsertItem(hWnd, &lvItem);

        lvItem.iSubItem = 1;
        std::wstring productNameStr = StringToWString(info.licenseDefinition.gameInfo.productName);
        lvItem.pszText = const_cast<LPWSTR>(productNameStr.c_str());
        ListView_SetItem(hWnd, &lvItem);

        lvItem.iSubItem = 2;
        lvItem.pszText = (info.licenseDefinition.variationType == ELicenseVariationType_Lite ? (LPWSTR)TEXT("Lite") : (LPWSTR)TEXT("Regular"));
        ListView_SetItem(hWnd, &lvItem);
        
        lvItem.iSubItem = 3;
        std::wstring accessTokenStr = StringToWString(info.accessToken);
        lvItem.pszText = const_cast<LPWSTR>(accessTokenStr.c_str());
        ListView_SetItem(hWnd, &lvItem);

        lvItem.iSubItem = 4;
        std::wstring createdTimeStr = TimePointToWString(info.accessTime);
        lvItem.pszText = const_cast<LPWSTR>(createdTimeStr.c_str());
        ListView_SetItem(hWnd, &lvItem);

        lvItem.iSubItem = 5;
        lvItem.pszText = (info.inUse == 0 ? (LPWSTR)TEXT("NO") : (LPWSTR)TEXT("YES"));
        ListView_SetItem(hWnd, &lvItem);
    }
}

void RunCommand(std::wstring command)
{
    // Create the process information structure
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Create the startup information structure
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    // Create the new process
    BOOL success = CreateProcessW(
        nullptr,   // Application name (NULL means use command)
        const_cast<LPWSTR>(command.c_str()),  // Command line
        nullptr,   // Process security attributes
        nullptr,   // Thread security attributes
        FALSE,     // Inherit handles
        0,         // Creation flags
        nullptr,   // Use parent's environment
        nullptr,   // Use parent's current directory
        &si,       // Startup info
        &pi        // Process information
    );
}

// Show item information in a message box
void RunEngine(HWND hWnd, int itemIndex) {
    
    WCHAR buffer[256];

    ListView_GetItemText(hWnd, itemIndex, 0, buffer, 256);
    std::string licenseID = WCharToString(buffer);

    ListView_GetItemText(hWnd, itemIndex, 1, buffer, 256);
    std::string name = WCharToString(buffer);

    ListView_GetItemText(hWnd, itemIndex, 2, buffer, 256);
    std::string type = WCharToString(buffer);

    ListView_GetItemText(hWnd, itemIndex, 3, buffer, 256);
    std::string token = WCharToString(buffer);
    
    std::string engineCommand = "EngineApp.exe " + gClient->GetHostAddress() + " " + gClient->GetUserID() + " " + licenseID + " " + token;

    LicenseInstance & instance = gClient->GetLicenseInstance(std::stoi(licenseID));

    //if (instance.inUse == 0) {
        //std::system(engineCommand.c_str());
        std::wstring command = convertToWString(engineCommand);
        RunCommand(command);
    //}
}