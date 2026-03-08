#include "searchable_dropdown.hpp"
#include <algorithm>
#include <cctype>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
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

} // namespace

ftxui::Component SearchableDropdown(ftxui::ConstStringListRef entries,
                                    int *selected) {
  using namespace ftxui;

  struct State {
    bool open = false;
    std::string filter;
    std::string title;
    std::vector<std::string> filtered_entries;
    std::vector<int> filtered_indices;
    int selected_in_filtered = 0;
  };
  auto state = std::make_shared<State>();

  auto checkbox = Checkbox(&state->title, &state->open);
  auto filter_display = Renderer([state] {
    return hbox({text("Filter: "), text(state->filter), text("|")}) |
           borderEmpty;
  });

  auto radiobox = Radiobox(
      &state->filtered_entries, &state->selected_in_filtered,
      RadioboxOption{
          .transform =
              [](const EntryState &s) {
                auto prefix = text(s.state ? "↓ " : "→ ");
                auto t = text(s.label);
                if (s.active)
                  t |= bold;
                if (s.focused)
                  t |= inverted;
                return hbox({prefix, t});
              },
          .on_change =
              [state, selected] {
                if (!state->filtered_indices.empty() &&
                    state->selected_in_filtered >= 0 &&
                    state->selected_in_filtered <
                        int(state->filtered_indices.size()))
                  *selected =
                      state->filtered_indices[state->selected_in_filtered];
              },
      });

  auto inner = Container::Vertical({filter_display, radiobox});
  auto maybe_inner = Maybe(inner, [state] { return state->open; });

  class Impl : public ComponentBase {
  public:
    Impl(State *s, Component cb, Component inn,
         ftxui::ConstStringListRef entries, int *sel)
        : state_(s), checkbox_(std::move(cb)), inner_(std::move(inn)),
          entries_(entries), selected_(sel) {
      Add(Container::Vertical(
          {checkbox_, Maybe(inner_, [this] { return state_->open; })}));
    }

    void update_filtered() {
      state_->filtered_entries.clear();
      state_->filtered_indices.clear();
      for (size_t i = 0; i < entries_.size(); ++i) {
        std::string entry = entries_[i];
        if (contains_ignore_case(entry, state_->filter)) {
          state_->filtered_entries.push_back(entry);
          state_->filtered_indices.push_back(int(i));
        }
      }
      if (state_->filtered_entries.empty()) {
        state_->filtered_entries.push_back("(no matches)");
        state_->filtered_indices.push_back(-1);
      }
      int orig_selected = *selected_;
      state_->selected_in_filtered = 0;
      for (size_t i = 0; i < state_->filtered_indices.size(); ++i) {
        if (state_->filtered_indices[i] == orig_selected) {
          state_->selected_in_filtered = int(i);
          break;
        }
      }
      state_->selected_in_filtered =
          std::clamp(state_->selected_in_filtered, 0,
                     int(state_->filtered_entries.size()) - 1);
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

      if (open_old && state_->open) {
        const bool should_close =
            event == Event::Return || event == Event::Character(' ') ||
            (event.is_mouse() && event.mouse().button == Mouse::Left &&
             event.mouse().motion == Mouse::Pressed);
        if (should_close) {
          state_->open = false;
          state_->filter.clear();
          update_filtered();
          checkbox_->TakeFocus();
          handled = true;
        }
      }

      return handled;
    }

    Element OnRender() override {
      update_filtered();
      *selected_ = std::clamp(*selected_, 0, int(entries_.size()) - 1);
      if (*selected_ >= 0 && *selected_ < int(entries_.size()))
        state_->title = entries_[*selected_];

      const int max_height = 12;
      Element checkbox_el = checkbox_->Render();
      Element inner_el = inner_->Render();

      auto bdr = Focused() ? borderStyled(Color::Blue) : border;

      if (state_->open) {
        return vbox({
                   checkbox_el,
                   separator(),
                   inner_el | vscroll_indicator | frame |
                       size(HEIGHT, LESS_THAN, max_height),
               }) |
               bdr;
      }
      return vbox({checkbox_el, filler()}) | bdr;
    }

  private:
    State *state_;
    Component checkbox_;
    Component inner_;
    ftxui::ConstStringListRef entries_;
    int *selected_;
  };

  return ftxui::Make<Impl>(state.get(), checkbox, inner, entries, selected);
}

} // namespace malla
