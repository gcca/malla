#include "data_table.hpp"
#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <memory>

namespace malla {

ftxui::Component DataTable(const std::vector<std::string> *headers,
                           const std::vector<std::vector<std::string>> *rows,
                           const std::string *table_name,
                           const std::vector<std::string> *row_ids,
                           const std::string *pk_col,
                           const std::vector<std::string> *pk_values,
                           UpdateCellFn on_update) {
  using namespace ftxui;

  struct State {
    float scroll_x = 0.f;
    float scroll_y = 0.f;
    int cursor_row = 0;
    int cursor_col = 0;
    int edit_row = -1;
    int edit_col = -1;
    std::string edit_value;
    bool show_modal = false;
    bool select_all = false;
    std::string modal_old;
  };
  auto state = std::make_shared<State>();

  class Impl : public ComponentBase {
  public:
    Impl(const std::vector<std::string> *h,
         const std::vector<std::vector<std::string>> *r,
         const std::string *table_name, const std::vector<std::string> *row_ids,
         const std::string *pk_col, const std::vector<std::string> *pk_values,
         UpdateCellFn on_update, std::shared_ptr<State> st)
        : headers_(h), rows_(r), table_name_(table_name), row_ids_(row_ids),
          pk_col_(pk_col), pk_values_(pk_values),
          on_update_(std::move(on_update)), state_(std::move(st)) {
      input_ = Input(&state_->edit_value, "");
      Add(input_);
    }

    void start_edit(int row, int col) {
      int nrows = rows_ ? int(rows_->size()) : 0;
      int ncols = headers_ ? int(headers_->size()) : 0;
      if (row < 0 || row >= nrows || col < 0 || col >= ncols)
        return;
      state_->cursor_row = row;
      state_->cursor_col = col;
      state_->edit_row = row;
      state_->edit_col = col;
      const auto &r = (*rows_)[row];
      state_->edit_value = col < int(r.size()) ? r[col] : "";
      state_->show_modal = false;
      state_->select_all = true;
      input_->TakeFocus();
    }

    void trigger_modal() {
      if (state_->edit_row < 0)
        return;
      const auto &r = (*rows_)[state_->edit_row];
      int col = state_->edit_col;
      state_->modal_old = col < int(r.size()) ? r[col] : "";
      state_->show_modal = true;
    }

    void confirm_edit() {
      if (on_update_ && state_->edit_row >= 0 && state_->edit_col >= 0) {
        std::string rid;
        if (row_ids_ && state_->edit_row < int(row_ids_->size()))
          rid = (*row_ids_)[state_->edit_row];
        std::string col = state_->edit_col < int(headers_->size())
                              ? (*headers_)[state_->edit_col]
                              : "";
        on_update_(*table_name_, rid, col, state_->edit_value);
      }
      cancel_edit();
    }

    void cancel_edit() {
      state_->edit_row = -1;
      state_->edit_col = -1;
      state_->edit_value.clear();
      state_->show_modal = false;
      state_->select_all = false;
    }

    bool OnEvent(Event event) override {
      int nrows = rows_ ? int(rows_->size()) : 0;
      int ncols = headers_ ? int(headers_->size()) : 0;

      if (event.is_mouse() && (event.mouse().button == Mouse::WheelUp ||
                               event.mouse().button == Mouse::WheelDown)) {
        if (state_->show_modal)
          return false;
        if (!CaptureMouse(event))
          return false;
        const float step = 0.15f;
        if (event.mouse().button == Mouse::WheelUp)
          state_->scroll_y = std::max(0.f, state_->scroll_y - step);
        else
          state_->scroll_y = std::min(1.f, state_->scroll_y + step);
        return true;
      }

      if (event.is_mouse() && event.mouse().button == Mouse::Left &&
          event.mouse().motion == Mouse::Pressed) {
        int mx = event.mouse().x;
        int my = event.mouse().y;

        if (state_->show_modal) {
          if (mx < modal_box_.x_min || mx > modal_box_.x_max ||
              my < modal_box_.y_min || my > modal_box_.y_max) {
            cancel_edit();
            return true;
          }
          return false;
        }

        if (mx >= data_box_.x_min && mx <= data_box_.x_max &&
            my >= data_box_.y_min && my <= data_box_.y_max) {
          int col = (mx - data_box_.x_min) / cell_width_;
          int row = (my - data_box_.y_min) / 2;

          if (row >= 0 && row < nrows && col >= 0 && col < ncols) {
            state_->cursor_row = row;
            state_->cursor_col = col;
            if (state_->edit_row >= 0 &&
                (row != state_->edit_row || col != state_->edit_col)) {
              trigger_modal();
              return true;
            }
            if (state_->edit_row < 0) {
              start_edit(row, col);
              return true;
            }
            return input_->OnEvent(event);
          }
        }
        return false;
      }

      if (event == Event::Return) {
        if (state_->show_modal) {
          confirm_edit();
          return true;
        }
        if (state_->edit_row >= 0) {
          trigger_modal();
          return true;
        }
        if (nrows > 0 && ncols > 0) {
          start_edit(state_->cursor_row, state_->cursor_col);
          return true;
        }
        return false;
      }

      if (event == Event::Escape) {
        if (state_->show_modal || state_->edit_row >= 0) {
          cancel_edit();
          return true;
        }
        return false;
      }

      if (state_->edit_row >= 0 && !state_->show_modal) {
        if (state_->select_all) {
          bool is_arrow = event == Event::ArrowLeft ||
                          event == Event::ArrowRight ||
                          event == Event::ArrowUp || event == Event::ArrowDown;
          state_->select_all = false;
          if (!is_arrow)
            state_->edit_value.clear();
        }
        return input_->OnEvent(event);
      }

      if (!state_->show_modal && nrows > 0 && ncols > 0) {
        auto move = [&](int dr, int dc) -> bool {
          state_->cursor_row =
              std::clamp(state_->cursor_row + dr, 0, nrows - 1);
          state_->cursor_col =
              std::clamp(state_->cursor_col + dc, 0, ncols - 1);
          if (nrows > 1)
            state_->scroll_y = float(state_->cursor_row) / float(nrows - 1);
          if (ncols > 1)
            state_->scroll_x = float(state_->cursor_col) / float(ncols - 1);
          return true;
        };
        if (event == Event::ArrowLeft)
          return move(0, -1);
        if (event == Event::ArrowRight)
          return move(0, +1);
        if (event == Event::ArrowUp)
          return move(-1, 0);
        if (event == Event::ArrowDown)
          return move(+1, 0);
        if (event == Event::Character('h'))
          return move(0, -1);
        if (event == Event::Character('j'))
          return move(+1, 0);
        if (event == Event::Character('k'))
          return move(-1, 0);
        if (event == Event::Character('l'))
          return move(0, +1);
        if (event == Event::Character('a'))
          return move(0, -1);
        if (event == Event::Character('s'))
          return move(+1, 0);
        if (event == Event::Character('d'))
          return move(-1, 0);
        if (event == Event::Character('f'))
          return move(0, +1);
      }

      return false;
    }

    Element OnRender() override {
      int ncols = headers_ ? int(headers_->size()) : 0;
      int nrows = rows_ ? int(rows_->size()) : 0;

      int cur_r = nrows > 0 ? std::clamp(state_->cursor_row, 0, nrows - 1) : 0;
      int cur_c = ncols > 0 ? std::clamp(state_->cursor_col, 0, ncols - 1) : 0;
      state_->cursor_row = cur_r;
      state_->cursor_col = cur_c;

      auto bdr = Focused() ? borderStyled(Color::Blue) : border;

      if (ncols == 0)
        return text("(no columns selected)") | bdr | flex;

      const int term_width = Terminal::Size().dimx;
      cell_width_ = std::max(10, (term_width - 4) / ncols);
      const int cw = cell_width_;

      auto cell_text = [cw](const std::string &s) {
        int avail = cw - 2;
        std::string t = int(s.length()) > avail
                            ? s.substr(0, std::max(0, avail - 2)) + ".."
                            : s;
        return text(" " + t + " ") | size(WIDTH, EQUAL, cw);
      };

      Elements header_cells;
      for (int c = 0; c < ncols; ++c) {
        std::string h = c < int(headers_->size()) ? (*headers_)[c] : "";
        header_cells.push_back(cell_text(h));
      }
      auto header_el = hbox(std::move(header_cells)) | bold;

      Elements data_rows;
      for (int r = 0; r < nrows; ++r) {
        Elements cells;
        const auto &row = (*rows_)[r];
        for (int c = 0; c < ncols; ++c) {
          bool is_edit = (r == state_->edit_row && c == state_->edit_col);
          bool is_cursor = (r == cur_r && c == cur_c);

          if (is_edit && !state_->show_modal) {
            if (state_->select_all) {
              cells.push_back(cell_text(state_->edit_value) |
                              bgcolor(Color::Blue) | color(Color::White));
            } else {
              cells.push_back(input_->Render() | size(WIDTH, EQUAL, cw) |
                              bgcolor(Color::Blue) | color(Color::White));
            }
          } else {
            std::string val = c < int(row.size()) ? row[c] : "";
            auto el = cell_text(is_edit ? state_->edit_value : val);
            if (is_cursor || is_edit)
              el = el | inverted;
            cells.push_back(el);
          }
        }
        data_rows.push_back(vbox({hbox(std::move(cells)), text("")}));
      }

      auto data_el = vbox(std::move(data_rows)) |
                     focusPositionRelative(state_->scroll_x, state_->scroll_y) |
                     hscroll_indicator | vscroll_indicator | frame | flex |
                     reflect(data_box_);

      auto inner = vbox({header_el, separator(), data_el});
      auto outer = inner | bdr | flex | reflect(grid_box_);

      if (state_->show_modal) {
        std::string rid;
        if (row_ids_ && state_->edit_row < int(row_ids_->size()))
          rid = (*row_ids_)[state_->edit_row];

        std::string pk_label = pk_col_ && !pk_col_->empty() ? *pk_col_ : "ctid";
        std::string pk_val;
        if (pk_col_ && !pk_col_->empty() && pk_values_ &&
            state_->edit_row < int(pk_values_->size()))
          pk_val = (*pk_values_)[state_->edit_row];
        else
          pk_val = rid;

        auto modal_content =
            vbox({
                text("Update cell?") | bold,
                separator(),
                hbox({text("Table:  "), text(table_name_ ? *table_name_ : "")}),
                hbox({text("Column: "), text(state_->edit_col < ncols
                                                 ? (*headers_)[state_->edit_col]
                                                 : "")}),
                hbox({text(pk_label + ": "), text(pk_val)}),
                hbox({text("Old:    "), text(state_->modal_old)}),
                hbox({text("New:    "), text(state_->edit_value)}),
                separator(),
                hbox({text("[Enter] Confirm") | flex, text("[Esc] Cancel")}),
            }) |
            border;

        return dbox({
            outer,
            modal_content | reflect(modal_box_) | center | clear_under,
        });
      }

      return outer;
    }

    bool Focusable() const override { return true; }

  private:
    const std::vector<std::string> *headers_;
    const std::vector<std::vector<std::string>> *rows_;
    const std::string *table_name_;
    const std::vector<std::string> *row_ids_;
    const std::string *pk_col_;
    const std::vector<std::string> *pk_values_;
    UpdateCellFn on_update_;
    std::shared_ptr<State> state_;
    Component input_;
    mutable Box grid_box_;
    mutable Box data_box_;
    mutable Box modal_box_;
    mutable int cell_width_ = 10;
  };

  return ftxui::Make<Impl>(headers, rows, table_name, row_ids, pk_col,
                           pk_values, std::move(on_update), state);
}

} // namespace malla
