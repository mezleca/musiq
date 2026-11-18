#pragma once

#include <string>

namespace ui {
    class Surface;
}

std::string open_login(ui::Surface& surface);
void clear_browser_cookies(ui::Surface& surface);
