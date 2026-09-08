/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace earblaster {

/* Navy well: ice ring + upright bolt + EARBLASTER pill.
 * While playing, a tracer bead runs the ring; the bolt stays upright.
 */
class SealView : public Gtk::DrawingArea {
 public:
  SealView();
  ~SealView() override;

  void set_playing(bool playing);
  void stop();
  bool playing() const { return playing_; }

 protected:
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

 private:
  void draw_ring(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_bead(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_bolt(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_pill(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double y,
                 double width) const;

  bool on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock);
  void start_ticking();
  void stop_ticking();

  bool playing_ = false;
  bool show_bead_ = false;
  double angle_ = 0.0; /* 0 = top of ring; clockwise */
  guint tick_id_ = 0;
  gint64 last_tick_us_ = 0;
};

}  // namespace earblaster
