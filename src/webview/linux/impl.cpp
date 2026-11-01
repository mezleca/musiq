#include "impl.hpp"
#include "../../utils/system.hpp"

#include <utility>

std::unique_ptr<WebView> WebView::create(std::string_view title) {
    return std::make_unique<WebViewImpl>(title);
}

WebViewImpl::WebViewImpl(std::string_view title) : WebView(title) {
    auto* browser = static_cast<WebKitWebView*>(m_window.browser_controller().value());
    auto* settings = webkit_web_view_get_settings(browser);

    webkit_settings_set_user_agent(
        settings, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
                  "(KHTML, like Gecko) Version/17.4.1 Safari/605.1.15"
    );
    webkit_settings_set_enable_webgl(settings, false);

    const auto storage = utils::get_app_dir() / "login-cookies.sqlite";
    auto* manager = webkit_web_context_get_cookie_manager(webkit_web_view_get_context(browser));
    webkit_cookie_manager_set_persistent_storage(manager, storage.c_str(), WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
}

void WebViewImpl::start_poll() {
    m_timer = g_timeout_add(
        POLL_INTERVAL_MS,
        +[](gpointer data) -> gboolean {
            static_cast<WebViewImpl*>(data)->m_poll();
            return G_SOURCE_CONTINUE;
        },
        this
    );
}

void WebViewImpl::stop_poll() {
    g_source_remove(m_timer);
    m_timer = 0;
}

std::string WebViewImpl::url() {
    auto* browser = static_cast<WebKitWebView*>(m_window.browser_controller().value());
    const char* uri = webkit_web_view_get_uri(browser);
    return uri ? uri : "";
}

void WebViewImpl::read_cookies(const std::string& url, std::function<void(std::string)> callback) {
    auto* browser = static_cast<WebKitWebView*>(m_window.browser_controller().value());
    auto* manager = webkit_web_context_get_cookie_manager(webkit_web_view_get_context(browser));
    auto* completion = new std::function<void(std::string)>(std::move(callback));

    webkit_cookie_manager_get_cookies(
        manager, url.c_str(), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            const std::unique_ptr<std::function<void(std::string)>> complete(
                static_cast<std::function<void(std::string)>*>(data)
            );
            GError* error = nullptr;
            GList* cookies = webkit_cookie_manager_get_cookies_finish(WEBKIT_COOKIE_MANAGER(source), result, &error);
            std::string header;
            for (auto* entry = cookies; entry; entry = entry->next) {
                auto* cookie = static_cast<SoupCookie*>(entry->data);
                if (!header.empty()) {
                    header += "; ";
                }
                header += soup_cookie_get_name(cookie);
                header += '=';
                header += soup_cookie_get_value(cookie);
            }
            g_list_free_full(cookies, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
            g_clear_error(&error);
            (*complete)(std::move(header));
        },
        completion
    );
}
