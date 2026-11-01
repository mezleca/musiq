#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <webview/webview.h>

class WebView {
public:
    static std::unique_ptr<WebView> create(std::string_view title);

    virtual ~WebView() = default;
    WebView(const WebView&) = delete;
    WebView& operator=(const WebView&) = delete;
    WebView(WebView&&) = delete;
    WebView& operator=(WebView&&) = delete;

    // runs a window and polls on its ui thread every 100 ms
    void run(std::string_view url, std::function<void()> poll);
    void close();

    virtual std::string url() = 0;

    void cookies(std::string_view url, std::function<void(std::string)> callback);

protected:
    explicit WebView(std::string_view title);

    virtual void start_poll() = 0;
    virtual void stop_poll() = 0;
    virtual void read_cookies(const std::string& url, std::function<void(std::string)> callback) = 0;

    static constexpr unsigned int POLL_INTERVAL_MS = 100;

    webview::webview m_window{false, nullptr};
    std::function<void()> m_poll;

private:
    std::shared_ptr<bool> m_active;
};
