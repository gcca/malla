#pragma once

#include <algorithm>
#include <cctype>
#include <ftxui/component/component.hpp>
#include <ftxui/util/ref.hpp>
#include <string>

namespace malla {

ftxui::Component SearchableDropdown(ftxui::ConstStringListRef entries,
                                    int *selected);

}
