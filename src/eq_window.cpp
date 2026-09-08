/* SPDX-License-Identifier: Unlicense */

#include "eq_window.hpp"

#include <cstdio>

namespace earblaster {
namespace {

const char* kBandLabels[Settings::kEqBands] = {
    "29", "59", "119", "237", "470", "1k", "2k", "4k", "8k", "16k"};

}  // namespace

EqWindow::EqWindow(Player& player, Settings& settings)
    : player_(player), settings_(settings)
{
  set_title("Equalizer");
  set_border_width(8);
  set_resizable(false);
  set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);

  if (!player_.has_eq()) {
    auto* msg = Gtk::manage(new Gtk::Label("equalizer-10bands is not available."));
    add(*msg);
    show_all();
    return;
  }

  for (int i = 0; i < Settings::kEqBands; ++i) {
    auto* col = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 2));
    auto* scale = Gtk::manage(new Gtk::Scale(Gtk::ORIENTATION_VERTICAL));
    scale->set_range(-24.0, 12.0);
    scale->set_increments(0.5, 1.0);
    scale->set_inverted(true);
    scale->set_draw_value(false);
    scale->set_size_request(28, 160);
    scale->set_value(player_.eq_band(i));
    scale->signal_value_changed().connect([this, i]() { on_band(i); });
    scales_[static_cast<std::size_t>(i)] = scale;
    auto* lab = Gtk::manage(new Gtk::Label(kBandLabels[i]));
    col->pack_start(*scale, Gtk::PACK_EXPAND_WIDGET);
    col->pack_start(*lab, Gtk::PACK_SHRINK);
    bands_.pack_start(*col, Gtk::PACK_EXPAND_WIDGET);
  }

  reset_.signal_clicked().connect(sigc::mem_fun(*this, &EqWindow::on_reset));
  close_.signal_clicked().connect([this]() { hide(); });
  buttons_.pack_end(close_, Gtk::PACK_SHRINK);
  buttons_.pack_end(reset_, Gtk::PACK_SHRINK);

  root_.pack_start(bands_, Gtk::PACK_EXPAND_WIDGET);
  root_.pack_start(buttons_, Gtk::PACK_SHRINK);
  add(root_);
  show_all();

  signal_delete_event().connect([this](GdkEventAny*) {
    hide();
    return true;
  });
  signal_hide().connect([this]() { settings_.save(); });
}

void EqWindow::on_band(int i)
{
  if (ignore_ || !scales_[static_cast<std::size_t>(i)])
    return;
  const double v = scales_[static_cast<std::size_t>(i)]->get_value();
  player_.set_eq_band(i, v);
  settings_.eq[i] = player_.eq_band(i);
}

void EqWindow::on_reset()
{
  ignore_ = true;
  for (int i = 0; i < Settings::kEqBands; ++i) {
    player_.set_eq_band(i, 0.0);
    settings_.eq[i] = 0.0;
    if (scales_[static_cast<std::size_t>(i)])
      scales_[static_cast<std::size_t>(i)]->set_value(0.0);
  }
  ignore_ = false;
}

}  // namespace earblaster
