#include "impl.hpp"

#include <wrl.h>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

static std::string take_utf8(LPWSTR text) {
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> owner(text, CoTaskMemFree);
    if (!text) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return {};
    }

    std::string value(size, '\0');
    if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), size, nullptr, nullptr)) {
        return {};
    }

    value.pop_back();
    return value;
}

std::unique_ptr<WebView> WebView::create(std::string_view title) {
    return std::make_unique<WebViewImpl>(title);
}

WebViewImpl::WebViewImpl(std::string_view title) : WebView(title) {}

void WebViewImpl::clear_cookies() {
    ShowWindow(static_cast<HWND>(m_window.window().value()), SW_HIDE);
    auto* controller = static_cast<ICoreWebView2Controller*>(m_window.browser_controller().value());
    ComPtr<ICoreWebView2> browser;
    ComPtr<ICoreWebView2_2> browser2;
    ComPtr<ICoreWebView2CookieManager> manager;
    if (FAILED(controller->get_CoreWebView2(&browser)) || FAILED(browser.As(&browser2)) ||
        FAILED(browser2->get_CookieManager(&manager)) || FAILED(manager->DeleteAllCookies())) {
        throw std::runtime_error("could not clear browser cookies");
    }
}

void CALLBACK WebViewImpl::poll(HWND handle, UINT, UINT_PTR, DWORD) {
    auto* view = static_cast<WebViewImpl*>(GetPropW(handle, L"musiq.webview"));
    if (view) {
        view->m_poll();
    }
}

void WebViewImpl::start_poll() {
    const auto handle = static_cast<HWND>(m_window.window().value());
    if (!SetPropW(handle, L"musiq.webview", this)) {
        throw std::runtime_error("could not initialize the browser timer");
    }

    m_timer = SetTimer(handle, 0, POLL_INTERVAL_MS, poll);
    if (!m_timer) {
        RemovePropW(handle, L"musiq.webview");
        throw std::runtime_error("could not start the browser timer");
    }
}

void WebViewImpl::stop_poll() {
    const auto window = m_window.window();
    if (window.ok()) {
        const auto handle = static_cast<HWND>(window.value());
        KillTimer(handle, m_timer);
        RemovePropW(handle, L"musiq.webview");
    }

    m_timer = 0;
}

std::string WebViewImpl::url() {
    auto* controller = static_cast<ICoreWebView2Controller*>(m_window.browser_controller().value());
    ComPtr<ICoreWebView2> browser;
    LPWSTR source = nullptr;
    if (FAILED(controller->get_CoreWebView2(&browser)) || FAILED(browser->get_Source(&source))) {
        return {};
    }

    return take_utf8(source);
}

void WebViewImpl::read_cookies(const std::string& url, std::function<void(std::string)> callback) {
    auto* controller = static_cast<ICoreWebView2Controller*>(m_window.browser_controller().value());

    ComPtr<ICoreWebView2> browser;
    ComPtr<ICoreWebView2_2> browser2;
    ComPtr<ICoreWebView2CookieManager> manager;

    if (FAILED(controller->get_CoreWebView2(&browser)) || FAILED(browser.As(&browser2)) ||
        FAILED(browser2->get_CookieManager(&manager))) {
        callback({});
        return;
    }

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.c_str(), -1, nullptr, 0);
    if (!length) {
        callback({});
        return;
    }

    std::wstring uri(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.c_str(), -1, uri.data(), length);

    const auto completion =
        Callback<ICoreWebView2GetCookiesCompletedHandler>([callback](HRESULT error, ICoreWebView2CookieList* cookies) -> HRESULT {
            std::string header;
            UINT count = 0;
            if (SUCCEEDED(error) && cookies && SUCCEEDED(cookies->get_Count(&count))) {
                for (UINT i = 0; i < count; ++i) {
                    ComPtr<ICoreWebView2Cookie> cookie;
                    if (FAILED(cookies->GetValueAtIndex(i, &cookie))) {
                        continue;
                    }

                    LPWSTR name = nullptr;
                    LPWSTR value = nullptr;
                    const auto name_result = cookie->get_Name(&name);
                    const auto value_result = cookie->get_Value(&value);
                    auto key = take_utf8(name);
                    auto content = take_utf8(value);
                    if (FAILED(name_result) || FAILED(value_result)) {
                        continue;
                    }

                    if (!header.empty()) header += "; ";
                    header += key + '=' + content;
                }
            }
            callback(std::move(header));
            return S_OK;
        });

    if (FAILED(manager->GetCookies(uri.c_str(), completion.Get()))) {
        callback({});
    }
}
