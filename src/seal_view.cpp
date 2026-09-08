/* SPDX-License-Identifier: Unlicense */

#include "seal_view.hpp"

#include <cmath>

namespace earblaster {
namespace {

constexpr double kNavyR = 11.0 / 255.0;
constexpr double kNavyG = 29.0 / 255.0;
constexpr double kNavyB = 56.0 / 255.0;
constexpr double kIceR = 232.0 / 255.0;
constexpr double kIceG = 242.0 / 255.0;
constexpr double kIceB = 255.0 / 255.0;
constexpr double kGlowR = 126.0 / 255.0;
constexpr double kGlowG = 200.0 / 255.0;
constexpr double kGlowB = 227.0 / 255.0;

}  // namespace

SealView::SealView()
{
  set_size_request(246, 246);
  set_hexpand(false);
  set_vexpand(false);
  get_style_context()->add_class("earblaster-well");
}

void SealView::set_playing(bool playing)
{
  if (playing_ == playing)
    return;
  playing_ = playing;
  if (!playing_)
    angle_ = 0.0;
  queue_draw();
}

void SealView::draw_ring(const Cairo::RefPtr<Cairo::Context>& cr, double cx,
                         double cy, double radius) const
{
  cr->save();
  cr->translate(cx, cy);
  cr->rotate(angle_);
  cr->translate(-cx, -cy);

  cr->set_source_rgba(kGlowR, kGlowG, kGlowB, 0.55);
  cr->set_line_width(radius * 0.072);
  cr->arc(cx, cy, radius, 0, 2.0 * M_PI);
  cr->stroke();

  cr->set_source_rgb(kIceR, kIceG, kIceB);
  cr->set_line_width(radius * 0.045);
  cr->arc(cx, cy, radius, 0, 2.0 * M_PI);
  cr->stroke();

  cr->restore();
}

void SealView::draw_bolt(const Cairo::RefPtr<Cairo::Context>& cr, double cx,
                         double cy, double radius) const
{
  const double scale = radius / 168.0;
  cr->save();
  cr->translate(cx, cy);
  cr->scale(scale, scale);
  cr->translate(-256.0, -256.0);
  cr->move_to(268, 118);
  cr->line_to(196, 268);
  cr->line_to(250, 268);
  cr->line_to(232, 394);
  cr->line_to(328, 236);
  cr->line_to(270, 236);
  cr->close_path();
  cr->set_source_rgb(kIceR, kIceG, kIceB);
  cr->fill();
  cr->restore();
}

void SealView::draw_pill(const Cairo::RefPtr<Cairo::Context>& cr, double cx,
                         double y, double width) const
{
  const double h = 22.0;
  const double x = cx - width * 0.5;
  const double r = h * 0.5;
  cr->save();
  cr->set_source_rgb(kIceR, kIceG, kIceB);
  cr->set_line_width(2.0);
  cr->begin_new_sub_path();
  cr->arc(x + r, y + r, r, M_PI / 2.0, 3.0 * M_PI / 2.0);
  cr->arc(x + width - r, y + r, r, 3.0 * M_PI / 2.0, M_PI / 2.0);
  cr->close_path();
  cr->stroke();

  cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
  cr->set_font_size(11.0);
  Cairo::TextExtents ext;
  const char* label = "EARBLASTER";
  cr->get_text_extents(label, ext);
  cr->move_to(cx - ext.width * 0.5 - ext.x_bearing,
              y + h * 0.5 - ext.height * 0.5 - ext.y_bearing);
  cr->fill();
  cr->restore();
}

bool SealView::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  const Gtk::Allocation alloc = get_allocation();
  const double w = alloc.get_width();
  const double h = alloc.get_height();

  cr->set_source_rgb(kNavyR, kNavyG, kNavyB);
  cr->rectangle(0, 0, w, h);
  cr->fill();

  const double cx = w * 0.5;
  const double cy = h * 0.42;
  const double radius = std::min(w, h) * 0.28;
  draw_ring(cr, cx, cy, radius);
  draw_bolt(cr, cx, cy, radius);
  draw_pill(cr, cx, cy + radius + 18.0, 138.0);
  return true;
}

}  // namespace earblaster
