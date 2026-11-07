#pragma once

#include "../webview.hpp"

class WebViewImpl final : public WebView {
public:
    explicit WebViewImpl(std::string_view title);
    std::string url() override;
    void clear_cookies() override;

private:
    void start_poll() override;
    void stop_poll() override;
    void read_cookies(const std::string& url, std::function<void(std::string)> callback) override;

    guint m_timer = 0;
};
