#pragma once

#include <ftxui/component/component.hpp>
#include <functional>
#include <string>
#include <vector>

namespace malla {

using UpdateCellFn = std::function<bool(
    const std::string &table, const std::string &row_id,
    const std::string &column, const std::string &new_value)>;

ftxui::Component DataTable(const std::vector<std::string> *headers,
                           const std::vector<std::vector<std::string>> *rows,
                           const std::string *table_name,
                           const std::vector<std::string> *row_ids,
                           const std::string *pk_col,
                           const std::vector<std::string> *pk_values,
                           UpdateCellFn on_update);

} // namespace malla
