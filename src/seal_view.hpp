/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace earblaster {

/* Navy well: ice ring + upright bolt + EARBLASTER pill.
 * Play: bead laps once, then embedded cover stretch-fills the well if set.
 */
class SealView : public Gtk::DrawingArea {
 public:
  SealView();
  ~SealView() override;

  void set_playing(bool playing);
  void stop();
  void set_cover(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf);
  bool playing() const { return playing_; }

 protected:
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
  void on_size_allocate(Gtk::Allocation& allocation) override;

 private:
  void draw_ring(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_bead(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_bolt(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_pill(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double y,
                 double width) const;
  void recache_cover();
  bool cover_visible() const;

  bool on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock);
  void start_ticking();
  void stop_ticking();

  bool playing_ = false;
  bool show_bead_ = false;
  double angle_ = 0.0; /* 0 = top of ring; clockwise */
  guint tick_id_ = 0;
  gint64 last_tick_us_ = 0;
  double intro_spun_ = 0.0;
  bool intro_done_ = false;
  Glib::RefPtr<Gdk::Pixbuf> cover_;
  Glib::RefPtr<Gdk::Pixbuf> cover_scaled_;
};

}  // namespace earblaster
