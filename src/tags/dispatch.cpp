#include "tags/dispatch.hpp"

#include <algorithm>

#include "events/signal.hpp"
#include "events/signal_emitter.hpp"
#include "settings.hpp"
#include "tags/parser.hpp"
#include "utils/color.hpp"
#include "utils/factory.hpp"

POLYBAR_NS

using namespace signals::parser;

namespace tags {
  /**
   * Create instance
   */
  dispatch::make_type dispatch::make(action_context& action_ctxt) {
    return factory_util::unique<dispatch>(signal_emitter::make(), logger::make(), action_ctxt);
  }

  /**
   * Construct parser instance
   */
  dispatch::dispatch(signal_emitter& emitter, const logger& logger, action_context& action_ctxt)
      : m_sig(emitter), m_log(logger), m_action_ctxt(action_ctxt) {}

  /**
   * Process input string
   */
  void dispatch::parse(const bar_settings& bar, renderer_interface& renderer, const string&& data) {
    tags::parser p;
    p.set(std::move(data));

    m_action_ctxt.reset();
    m_ctxt = make_unique<context>(bar);

    while (p.has_next_element()) {
      tags::element el;
      try {
        el = p.next_element();
      } catch (const tags::error& e) {
        m_log.err("Parser error (reason: %s)", e.what());
        continue;
      }

      if (el.is_tag) {
        switch (el.tag_data.type) {
          case tags::tag_type::FORMAT:
            switch (el.tag_data.subtype.format) {
              case tags::syntaxtag::A:
                handle_action(renderer, el.tag_data.action.btn, el.tag_data.action.closing, std::move(el.data));
                break;
              case tags::syntaxtag::B:
                m_ctxt->apply_bg(el.tag_data.color);
                break;
              case tags::syntaxtag::F:
                m_ctxt->apply_fg(el.tag_data.color);
                break;
              case tags::syntaxtag::T:
                m_ctxt->apply_font(el.tag_data.font);
                break;
              case tags::syntaxtag::O:
                renderer.render_offset(*m_ctxt, el.tag_data.offset);
                break;
              case tags::syntaxtag::R:
                m_ctxt->apply_reverse();
                break;
              case tags::syntaxtag::o:
                m_ctxt->apply_ol(el.tag_data.color);
                break;
              case tags::syntaxtag::u:
                m_ctxt->apply_ul(el.tag_data.color);
                break;
              case tags::syntaxtag::P:
                handle_control(el.tag_data.ctrl);
                break;
              case tags::syntaxtag::l:
                m_ctxt->apply_alignment(alignment::LEFT);
                m_sig.emit(change_alignment{alignment::LEFT});
                break;
              case tags::syntaxtag::r:
                m_ctxt->apply_alignment(alignment::RIGHT);
                m_sig.emit(change_alignment{alignment::RIGHT});
                break;
              case tags::syntaxtag::c:
                m_ctxt->apply_alignment(alignment::CENTER);
                m_sig.emit(change_alignment{alignment::CENTER});
                break;
              default:
                throw runtime_error(
                    "Unrecognized tag format: " + to_string(static_cast<int>(el.tag_data.subtype.format)));
            }
            break;
          case tags::tag_type::ATTR:
            m_ctxt->apply_attr(el.tag_data.subtype.activation, el.tag_data.attr);
            break;
        }
      } else {
        handle_text(renderer, std::move(el.data));
      }
    }

    // TODO handle unclosed action tags in ctxt
    /* if (!m_actions.empty()) { */
    /*   throw runtime_error(to_string(m_actions.size()) + " unclosed action block(s)"); */
    /* } */
  }

  /**
   * Process text contents
   */
  void dispatch::handle_text(renderer_interface& renderer, string&& data) {
#ifdef DEBUG_WHITESPACE
    string::size_type p;
    while ((p = data.find(' ')) != string::npos) {
      data.replace(p, 1, "-"s);
    }
#endif

    renderer.render_text(*m_ctxt, std::move(data));
  }

  void dispatch::handle_action(renderer_interface& renderer, mousebtn btn, bool closing, const string&& cmd) {
    if (closing) {
      action_t id;
      std::tie(id, btn) = m_action_ctxt.action_close(btn, m_ctxt->get_alignment());
      renderer.action_close(*m_ctxt, id);
    } else {
      string tmp = cmd;
      action_t id = m_action_ctxt.action_open(btn, std::move(cmd), m_ctxt->get_alignment());
      renderer.action_open(*m_ctxt, btn, id);
    }
  }

  void dispatch::handle_control(controltag ctrl) {
    switch (ctrl) {
      case controltag::R:
        m_ctxt->apply_reset();
        break;
      default:
        throw runtime_error("Unrecognized polybar control tag: " + to_string(static_cast<int>(ctrl)));
    }
  }

}  // namespace tags

POLYBAR_NS_END