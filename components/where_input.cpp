#include "where_input.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <memory>

namespace malla {

ftxui::Component WhereInput(std::string *value,
                            std::function<void()> on_enter) {
  using namespace ftxui;

  struct State {
    bool editing = false;
  };
  auto state = std::make_shared<State>();

  class Impl : public ComponentBase {
  public:
    Impl(std::string *value, std::function<void()> on_enter,
         std::shared_ptr<State> st)
        : value_(value), on_enter_(std::move(on_enter)), state_(std::move(st)) {
      input_ = Input(value_, "");
      Add(input_);
    }

    bool OnEvent(Event event) override {
      if (!state_->editing) {
        if (event == Event::Return) {
          state_->editing = true;
          input_->TakeFocus();
          return true;
        }
        return false;
      }

      if (event == Event::Return) {
        state_->editing = false;
        if (on_enter_)
          on_enter_();
        return true;
      }
      if (event == Event::Escape) {
        state_->editing = false;
        return true;
      }
      return input_->OnEvent(event);
    }

    Element OnRender() override {
      auto bdr = Focused() ? borderStyled(Color::Blue) : border;
      const int min_w = Terminal::Size().dimx / 2;
      Element content;
      if (state_->editing) {
        content = hbox({text("Filter ") | dim, input_->Render() | flex});
      } else {
        content = hbox({text("Filter ") | dim, text(*value_) | flex});
      }
      return content | bdr | size(WIDTH, GREATER_THAN, min_w);
    }

    bool Focusable() const override { return true; }

  private:
    std::string *value_;
    std::function<void()> on_enter_;
    std::shared_ptr<State> state_;
    Component input_;
  };

  return ftxui::Make<Impl>(value, std::move(on_enter), state);
}

} // namespace malla
