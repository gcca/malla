#pragma once

#include <ftxui/component/component.hpp>
#include <functional>
#include <string>

namespace malla {

ftxui::Component WhereInput(std::string *value, std::function<void()> on_enter);

} // namespace malla
