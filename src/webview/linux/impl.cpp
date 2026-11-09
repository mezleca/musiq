#include "../webview.hpp"
#include "../../utils/system.hpp"
#include "../../utils/log.hpp"

#include <utility>
#include <stdexcept>

class WebViewImpl final : public WebView {
public:
    explicit WebViewImpl(std::string_view title) : WebView(title) {
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

    void clear_cookies() override {
        gtk_widget_hide(GTK_WIDGET(m_window.window().value()));
        auto* browser = static_cast<WebKitWebView*>(m_window.browser_controller().value());
        auto* manager = webkit_web_context_get_website_data_manager(webkit_web_view_get_context(browser));
        auto* loop = g_main_loop_new(nullptr, false);
        GError* error = nullptr;
        std::pair<GMainLoop*, GError**> completion{loop, &error};
        webkit_website_data_manager_clear(
            manager, WEBKIT_WEBSITE_DATA_COOKIES, 0, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer data) {
                auto& [loop, error] = *static_cast<std::pair<GMainLoop*, GError**>*>(data);
                webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), result, error);
                g_main_loop_quit(loop);
            },
            &completion
        );
        g_main_loop_run(loop);
        g_main_loop_unref(loop);
        if (error) {
            const std::string message = error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }
    }

    std::string url() override {
        auto* browser = static_cast<WebKitWebView*>(m_window.browser_controller().value());
        const char* uri = webkit_web_view_get_uri(browser);
        return uri ? uri : "";
    }

private:
    void start_poll() override {
        m_timer = g_timeout_add(
            POLL_INTERVAL_MS,
            +[](gpointer data) -> gboolean {
                static_cast<WebViewImpl*>(data)->m_poll();
                return G_SOURCE_CONTINUE;
            },
            this
        );
        if (m_timer == 0) throw std::runtime_error("could not start the browser timer");
    }

    void stop_poll() override {
        if (m_timer != 0) g_source_remove(m_timer);
        m_timer = 0;
    }

    void read_cookies(const std::string& url, std::function<void(std::string)> callback) override {
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
                if (error) {
                    LOG_ERROR("[login] could not read browser cookies: {}", error->message);
                    g_clear_error(&error);
                }
                (*complete)(std::move(header));
            },
            completion
        );
    }

    guint m_timer = 0;
};

std::unique_ptr<WebView> WebView::create(std::string_view title) {
    return std::make_unique<WebViewImpl>(title);
}
