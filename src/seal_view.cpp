/* SPDX-License-Identifier: Unlicense */

#include "seal_view.hpp"

#include <gdkmm/general.h>

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

/* Tracer bead: one trip around the ring per second. 0 rad = 12 o'clock. */
constexpr double kRevPerSec = 1.0;
constexpr double kRadPerSec = kRevPerSec * 2.0 * M_PI;

}  // namespace

SealView::SealView()
{
  set_size_request(246, 246);
  set_hexpand(false);
  set_vexpand(false);
  get_style_context()->add_class("earblaster-well");
}

SealView::~SealView()
{
  stop_ticking();
}

void SealView::set_playing(bool playing)
{
  if (playing_ == playing)
    return;
  playing_ = playing;
  if (playing_) {
    if (cover_visible()) {
      show_bead_ = false;
    } else {
      show_bead_ = true;
      start_ticking();
    }
  } else {
    stop_ticking();
  }
  queue_draw();
}

void SealView::stop()
{
  playing_ = false;
  show_bead_ = false;
  angle_ = 0.0;
  intro_spun_ = 0.0;
  intro_done_ = false;
  stop_ticking();
  queue_draw();
}

void SealView::set_cover(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
{
  cover_ = pixbuf;
  recache_cover();
  if (cover_visible()) {
    show_bead_ = false;
    stop_ticking();
  }
  queue_draw();
}

void SealView::recache_cover()
{
  cover_scaled_.reset();
  if (!cover_)
    return;
  const Gtk::Allocation alloc = get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();
  if (w < 1 || h < 1)
    return;
  if (cover_->get_width() == w && cover_->get_height() == h)
    cover_scaled_ = cover_;
  else
    cover_scaled_ = cover_->scale_simple(w, h, Gdk::INTERP_BILINEAR);
}

bool SealView::cover_visible() const
{
  return static_cast<bool>(cover_) && intro_done_;
}

void SealView::on_size_allocate(Gtk::Allocation& allocation)
{
  Gtk::DrawingArea::on_size_allocate(allocation);
  recache_cover();
}

void SealView::start_ticking()
{
  if (tick_id_ != 0)
    return;
  last_tick_us_ = 0;
  tick_id_ = add_tick_callback(sigc::mem_fun(*this, &SealView::on_tick));
}

void SealView::stop_ticking()
{
  if (tick_id_ == 0)
    return;
  remove_tick_callback(tick_id_);
  tick_id_ = 0;
  last_tick_us_ = 0;
}

bool SealView::on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock)
{
  if (!playing_ || !clock)
    return true;

  const gint64 now = clock->get_frame_time();
  if (last_tick_us_ != 0) {
    const double dt = static_cast<double>(now - last_tick_us_) / 1e6;
    const double step = dt * kRadPerSec;
    angle_ = std::fmod(angle_ + step, 2.0 * M_PI);
    if (angle_ < 0.0)
      angle_ += 2.0 * M_PI;
    if (!intro_done_) {
      intro_spun_ += step;
      if (intro_spun_ >= 2.0 * M_PI)
        intro_done_ = true;
    }
    if (cover_visible()) {
      show_bead_ = false;
      last_tick_us_ = now;
      tick_id_ = 0;
      queue_draw();
      return false;
    }
    queue_draw();
  }
  last_tick_us_ = now;
  return true;
}

void SealView::draw_ring(const Cairo::RefPtr<Cairo::Context>& cr, double cx,
                         double cy, double radius) const
{
  cr->save();
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

void SealView::draw_bead(const Cairo::RefPtr<Cairo::Context>& cr, double cx,
                         double cy, double radius) const
{
  /* Cairo y-down: 0 rad is 3 o'clock. Subtract π/2 so 0 sits at the top. */
  const double theta = angle_ - M_PI / 2.0;
  const double x = cx + radius * std::cos(theta);
  const double y = cy + radius * std::sin(theta);
  const double r = radius * 0.045 * 1.8;

  cr->save();
  cr->set_source_rgba(1.0, 0.92, 0.2, 0.4);
  cr->arc(x, y, r * 1.85, 0, 2.0 * M_PI);
  cr->fill();
  cr->set_source_rgb(1.0, 0.85, 0.12);
  cr->arc(x, y, r, 0, 2.0 * M_PI);
  cr->fill();
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
  cr->show_text(label);
  cr->restore();
}

bool SealView::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  const Gtk::Allocation alloc = get_allocation();
  const double w = alloc.get_width();
  const double h = alloc.get_height();

  if (cover_visible() && cover_scaled_) {
    Gdk::Cairo::set_source_pixbuf(cr, cover_scaled_, 0, 0);
    cr->paint();
    return true;
  }

  cr->set_source_rgb(kNavyR, kNavyG, kNavyB);
  cr->rectangle(0, 0, w, h);
  cr->fill();

  const double cx = w * 0.5;
  const double cy = h * 0.42;
  const double radius = std::min(w, h) * 0.28;
  draw_ring(cr, cx, cy, radius);
  draw_bolt(cr, cx, cy, radius);
  if (show_bead_)
    draw_bead(cr, cx, cy, radius);
  draw_pill(cr, cx, cy + radius + 18.0, 138.0);
  return true;
}

}  // namespace earblaster
