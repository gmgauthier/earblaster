/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace earblaster {

/* Navy well: ice ring + upright bolt + EARBLASTER pill.
 * M0 draws a static mark. M1 will rotate the ring only.
 */
class SealView : public Gtk::DrawingArea {
 public:
  SealView();

  void set_playing(bool playing);
  bool playing() const { return playing_; }

 protected:
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

 private:
  void draw_ring(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_bolt(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double cy,
                 double radius) const;
  void draw_pill(const Cairo::RefPtr<Cairo::Context>& cr, double cx, double y,
                 double width) const;

  bool playing_ = false;
  double angle_ = 0.0; /* radians; always 0 in M0 */
};

}  // namespace earblaster
