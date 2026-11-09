#include "webview.hpp"
#include <utility>

WebView::WebView(std::string_view title) {
    m_window.set_title(std::string(title)).ensure_ok();
    m_window.set_size(480, 720, WEBVIEW_HINT_NONE).ensure_ok();
}

void WebView::run(std::string_view url, std::function<void()> poll) {
    m_window.navigate(std::string(url)).ensure_ok();

    m_poll = std::move(poll);
    m_active = std::make_shared<bool>(true);
    start_poll();
    auto result = m_window.run();
    *m_active = false;
    stop_poll();
    m_poll = {};

    result.ensure_ok();
}

void WebView::close() {
    m_window.terminate();
}

void WebView::cookies(std::string_view url, std::function<void(std::string)> callback) {
    read_cookies(std::string(url), [active = std::weak_ptr(m_active), callback = std::move(callback)](std::string cookies) {
        const auto running = active.lock();
        // closing the login window invalidates callbacks that captured its stack state
        if (running && *running) {
            callback(std::move(cookies));
        }
    });
}
