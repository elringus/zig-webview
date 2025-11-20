// Minimal C++ shim for WebView2. Exposes a small C API for Zig.

#include <Windows.h>
#include "WebView2.h"

static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;

class ControllerCompletedHandler;
class EnvironmentCompletedHandler;

class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG _ref;
public:
    ControllerCompletedHandler() : _ref(1) {}
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
        if (FAILED(result) || controller == nullptr) return result;
        g_controller = controller;
        g_controller->AddRef();
        if (SUCCEEDED(g_controller->get_CoreWebView2(&g_webview)) && g_webview) {
            g_webview->AddRef();
            // initial bounds can be set by the caller; nothing to do here
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
        if (FAILED(result) || env == nullptr) return result;
        // create controller for the provided parent window
        return env->CreateCoreWebView2Controller(_parent, new ControllerCompletedHandler());
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
        hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, envHandler);
        // Return the HRESULT so the caller can check for success (S_OK == 0)
        return (int)hr;
    }

    __declspec(dllexport) void webview2_navigate(const char* url) {
        if (!g_webview || !url) return;
        int len = MultiByteToWideChar(CP_UTF8, 0, url, -1, nullptr, 0);
        if (len == 0) return;
        wchar_t* wurl = (wchar_t*)CoTaskMemAlloc(sizeof(wchar_t) * len);
        if (!wurl) return;
        MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, len);
        g_webview->Navigate(wurl);
        CoTaskMemFree(wurl);
    }

    __declspec(dllexport) void webview2_set_visibility(int visible) {
        if (!g_controller) return;
        g_controller->put_IsVisible(visible ? TRUE : FALSE);
    }
}
