#include "columns_dropdown.hpp"
#include <algorithm>
#include <cctype>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>

namespace malla {

namespace {

bool contains_ignore_case(std::string_view haystack, std::string_view needle) {
  if (needle.empty())
    return true;
  if (haystack.size() < needle.size())
    return false;
  for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      if (std::tolower(static_cast<unsigned char>(haystack[i + j])) !=
          std::tolower(static_cast<unsigned char>(needle[j]))) {
        match = false;
        break;
      }
    }
    if (match)
      return true;
  }
  return false;
}

std::string column_label(const std::pair<std::string, std::string> &col) {
  return col.first + " (" + col.second + ")";
}

} // namespace

ftxui::Component
ColumnsDropdown(std::vector<std::pair<std::string, std::string>> *columns,
                std::vector<bool> *selected) {
  using namespace ftxui;

  struct State {
    bool open = false;
    std::string filter;
    std::string title;
    std::vector<int> filtered_indices;
    int hovered = 0;
  };
  auto state = std::make_shared<State>();

  auto checkbox = Checkbox(&state->title, &state->open);
  auto filter_display = Renderer([state] {
    return hbox({text("Filter: "), text(state->filter), text("|")}) |
           borderEmpty;
  });

  auto all_btn = Button("all", [columns, selected] {
    selected->resize(columns->size(), true);
    std::fill(selected->begin(), selected->end(), true);
  });
  auto none_btn = Button("none", [selected] {
    for (size_t i = 0; i < selected->size(); ++i)
      (*selected)[i] = false;
  });
  auto header_btns = Container::Horizontal({all_btn, none_btn});

  class ListImpl : public ComponentBase {
  public:
    ListImpl(State *s, std::vector<std::pair<std::string, std::string>> *cols,
             std::vector<bool> *sel)
        : state_(s), columns_(cols), selected_(sel) {}

    bool OnEvent(ftxui::Event event) override {
      if (!CaptureMouse(event))
        return false;

      if (event.is_mouse()) {
        for (size_t i = 0; i < boxes_.size(); ++i) {
          if (boxes_[i].Contain(event.mouse().x, event.mouse().y)) {
            if (event.mouse().button == Mouse::Left &&
                event.mouse().motion == Mouse::Pressed) {
              int idx = state_->filtered_indices[i];
              if (idx >= 0 && idx < int(selected_->size()))
                (*selected_)[idx] = !(*selected_)[idx];
              return true;
            }
          }
        }
        return false;
      }

      if (!Focused() || state_->filtered_indices.empty())
        return false;

      if (event == Event::ArrowUp || event == Event::Character('k')) {
        state_->hovered = std::max(0, state_->hovered - 1);
        return true;
      }
      if (event == Event::ArrowDown || event == Event::Character('j')) {
        state_->hovered = std::min(int(state_->filtered_indices.size()) - 1,
                                   state_->hovered + 1);
        return true;
      }
      if (event == Event::Character(' ') || event == Event::Return) {
        int idx = state_->filtered_indices[state_->hovered];
        if (idx >= 0 && idx < int(selected_->size()))
          (*selected_)[idx] = !(*selected_)[idx];
        return true;
      }
      return false;
    }

    Element OnRender() override {
      boxes_.clear();
      Elements elements;
      for (size_t i = 0; i < state_->filtered_indices.size(); ++i) {
        int idx = state_->filtered_indices[i];
        if (idx < 0 || idx >= int(columns_->size()) ||
            idx >= int(selected_->size()))
          continue;
        std::string label = column_label((*columns_)[idx]);
        bool checked = (*selected_)[idx];
        bool focused = (int(i) == state_->hovered) && Focused();
        auto el = hbox({text(checked ? "[x] " : "[ ] "), text(label)});
        if (focused)
          el |= inverted;
        boxes_.push_back(Box{});
        elements.push_back(std::move(el) | reflect(boxes_.back()));
      }
      if (elements.empty())
        return text("(no columns)");
      int n = int(state_->filtered_indices.size());
      float scroll_y = n > 1 ? float(state_->hovered) / float(n - 1) : 0.f;
      return vbox(std::move(elements)) | focusPositionRelative(0.f, scroll_y);
    }

    bool Focusable() const override {
      return !state_->filtered_indices.empty();
    }

  private:
    State *state_;
    std::vector<std::pair<std::string, std::string>> *columns_;
    std::vector<bool> *selected_;
    std::vector<Box> boxes_;
    Box box_;
  };

  auto list_impl = ftxui::Make<ListImpl>(state.get(), columns, selected);

  auto list_with_frame = list_impl | ftxui::vscroll_indicator | ftxui::frame |
                         ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 12);

  auto inner = Container::Vertical({
      header_btns,
      filter_display,
      list_with_frame,
  });
  auto maybe_inner = Maybe(inner, [state] { return state->open; });

  class Impl : public ComponentBase {
  public:
    Impl(State *s, Component cb, Component inn,
         std::vector<std::pair<std::string, std::string>> *cols,
         std::vector<bool> *sel)
        : state_(s), checkbox_(std::move(cb)), inner_(std::move(inn)),
          columns_(cols), selected_(sel) {
      Add(Container::Vertical(
          {checkbox_, Maybe(inner_, [this] { return state_->open; })}));
    }

    void update_filtered() {
      state_->filtered_indices.clear();
      for (size_t i = 0; i < columns_->size(); ++i) {
        std::string label = column_label((*columns_)[i]);
        if (contains_ignore_case(label, state_->filter))
          state_->filtered_indices.push_back(int(i));
      }
      state_->hovered =
          std::clamp(state_->hovered, 0,
                     std::max(0, int(state_->filtered_indices.size()) - 1));
    }

    void sync_selected_size() {
      if (selected_->size() != columns_->size()) {
        selected_->resize(columns_->size(), true);
        for (size_t i = 0; i < selected_->size(); ++i)
          (*selected_)[i] = true;
      }
    }

    void update_title() {
      sync_selected_size();
      size_t count = 0;
      for (bool b : *selected_)
        if (b)
          ++count;
      state_->title = "Columns (" + std::to_string(count) + "/" +
                      std::to_string(columns_->size()) + ")";
    }

    bool OnEvent(ftxui::Event event) override {
      const bool open_old = state_->open;

      if (state_->open) {
        if (event == Event::Escape) {
          state_->open = false;
          state_->filter.clear();
          update_filtered();
          checkbox_->TakeFocus();
          return true;
        }
        if (event.is_character() && event != Event::Return &&
            event != Event::Character(' ')) {
          state_->filter += event.character();
          update_filtered();
          return true;
        }
        if (event == Event::Backspace) {
          if (!state_->filter.empty()) {
            state_->filter.pop_back();
            update_filtered();
          }
          return true;
        }
      }

      bool handled = ComponentBase::OnEvent(event);

      if (!open_old && state_->open) {
        update_filtered();
        inner_->TakeFocus();
      }

      return handled;
    }

    Element OnRender() override {
      sync_selected_size();
      update_filtered();
      update_title();

      Element checkbox_el = checkbox_->Render();
      Element inner_el = inner_->Render();

      auto bdr = Focused() ? borderStyled(Color::Blue) : border;

      if (state_->open) {
        return vbox({
                   checkbox_el,
                   separator(),
                   inner_el,
               }) |
               bdr;
      }
      return vbox({checkbox_el, filler()}) | bdr;
    }

  private:
    State *state_;
    Component checkbox_;
    Component inner_;
    std::vector<std::pair<std::string, std::string>> *columns_;
    std::vector<bool> *selected_;
  };

  return ftxui::Make<Impl>(state.get(), checkbox, inner, columns, selected);
}

} // namespace malla
