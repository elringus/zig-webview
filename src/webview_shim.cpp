// Minimal C++ shim for WebView2. Exposes a small C API for Zig.

#include <Windows.h>
#include "WebView2.h"
#include <string>
#include <cstdio>

static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;
static std::wstring g_pending_navigation;
static HANDLE g_controller_event = nullptr;

class ControllerCompletedHandler;
class EnvironmentCompletedHandler;

class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG _ref;
    HWND _parent;
public:
    ControllerCompletedHandler(HWND parent) : _ref(1), _parent(parent) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef(void) override { return InterlockedIncrement(&_ref); }
    STDMETHODIMP_(ULONG) Release(void) override {
        LONG r = InterlockedDecrement(&_ref);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        printf("ControllerCompletedHandler::Invoke result=0x%08x\n", (unsigned)result);
        if (FAILED(result) || controller == nullptr) return result;
        g_controller = controller;
        g_controller->AddRef();
        // attach to parent window and size to client rect
        if (_parent) {
            HRESULT hrpw = g_controller->put_ParentWindow(_parent);
            printf("ControllerCompletedHandler: put_ParentWindow hr=0x%08x\n", (unsigned)hrpw);
            RECT rc; GetClientRect(_parent, &rc);
            printf("ControllerCompletedHandler: parent client rect left=%ld top=%ld right=%ld bottom=%ld\n", rc.left, rc.top, rc.right, rc.bottom);
            HRESULT hrb = g_controller->put_Bounds(rc);
            printf("ControllerCompletedHandler: put_Bounds hr=0x%08x\n", (unsigned)hrb);
            HRESULT hrv = g_controller->put_IsVisible(TRUE);
            printf("ControllerCompletedHandler: put_IsVisible hr=0x%08x\n", (unsigned)hrv);
            // Notify the controller that the parent window position may have changed so the
            // WebView can update its internal HWND and layout.
            HRESULT hrn = g_controller->NotifyParentWindowPositionChanged();
            printf("ControllerCompletedHandler: NotifyParentWindowPositionChanged hr=0x%08x\n", (unsigned)hrn);
        }
        if (SUCCEEDED(g_controller->get_CoreWebView2(&g_webview)) && g_webview) {
            g_webview->AddRef();
            // navigate pending URL if any
            if (!g_pending_navigation.empty()) {
                g_webview->Navigate(g_pending_navigation.c_str());
                g_pending_navigation.clear();
            }
        }
        if (g_controller_event) {
            SetEvent(g_controller_event);
            printf("ControllerCompletedHandler: signaled event\n");
        }
        return S_OK;
    }
};

class EnvironmentCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG _ref;
    HWND _parent;
public:
    EnvironmentCompletedHandler(HWND parent) : _ref(1), _parent(parent) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef(void) override { return InterlockedIncrement(&_ref); }
    STDMETHODIMP_(ULONG) Release(void) override {
        LONG r = InterlockedDecrement(&_ref);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        printf("EnvironmentCompletedHandler::Invoke result=0x%08x\n", (unsigned)result);
        if (FAILED(result) || env == nullptr) return result;
        // create controller for the provided parent window
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* handler = new ControllerCompletedHandler(_parent);
        HRESULT hr2 = env->CreateCoreWebView2Controller(_parent, handler);
        printf("CreateCoreWebView2Controller returned 0x%08x\n", (unsigned)hr2);
        return hr2;
    }
};

extern "C" {
    __declspec(dllexport) int webview2_init(void* hwnd_ptr) {
        if (!hwnd_ptr) return (int)E_INVALIDARG;
        HWND hwnd = (HWND)hwnd_ptr;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) return (int)hr;

        // Create environment and then controller via callbacks
        EnvironmentCompletedHandler* envHandler = new EnvironmentCompletedHandler(hwnd);
        // create an event to wait for controller completion
        if (!g_controller_event) g_controller_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!g_controller_event) return (int)HRESULT_FROM_WIN32(GetLastError());
        ResetEvent(g_controller_event);
        printf("Calling CreateCoreWebView2EnvironmentWithOptions\n");
        hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, envHandler);
        if (FAILED(hr)) return (int)hr;
        // wait up to 5 seconds for controller completed callback
        const DWORD timeout_ms = 5000;
        const DWORD start = GetTickCount();
        BOOL signaled = FALSE;
        while (GetTickCount() - start < timeout_ms) {
            DWORD elapsed = GetTickCount() - start;
            DWORD remaining = timeout_ms - elapsed;
            DWORD result = MsgWaitForMultipleObjects(1, &g_controller_event, FALSE, remaining, QS_ALLINPUT);
            if (result == WAIT_OBJECT_0) {
                signaled = TRUE;
                break;
            } else if (result == WAIT_OBJECT_0 + 1) {
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                // loop again until event or timeout
            } else if (result == WAIT_TIMEOUT) {
                break;
            } else {
                // error
                printf("webview2_init: MsgWaitForMultipleObjects error=0x%08x\n", (unsigned)GetLastError());
                break;
            }
        }
        if (signaled) return (int)S_OK;
        printf("webview2_init: controller creation timed out\n");
        return (int)HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }

    __declspec(dllexport) void webview2_navigate(const char* url) {
        if (!url) return;
        int len = MultiByteToWideChar(CP_UTF8, 0, url, -1, nullptr, 0);
        if (len == 0) return;
        std::wstring wurl(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, url, -1, &wurl[0], len);
        if (g_webview) {
            g_webview->Navigate(wurl.c_str());
        } else {
            // queue for when the webview is ready
            g_pending_navigation = wurl;
        }
    }

    __declspec(dllexport) void webview2_set_visibility(int visible) {
        if (!g_controller) return;
        g_controller->put_IsVisible(visible ? TRUE : FALSE);
    }
}
