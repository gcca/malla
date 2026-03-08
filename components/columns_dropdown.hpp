#pragma once

#include <ftxui/component/component.hpp>
#include <string>
#include <utility>
#include <vector>

namespace malla {

ftxui::Component
ColumnsDropdown(std::vector<std::pair<std::string, std::string>> *columns,
                std::vector<bool> *selected);

}
