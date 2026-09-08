/* SPDX-License-Identifier: Unlicense */

#include "player.hpp"

#include <gst/tag/tag.h>

#include <gdkmm/pixbufloader.h>

namespace earblaster {
namespace {

std::string path_to_uri(const std::string& path)
{
  if (path.compare(0, 5, "file:") == 0 || path.compare(0, 4, "http") == 0)
    return path;
  GError* err = nullptr;
  gchar* uri = gst_filename_to_uri(path.c_str(), &err);
  if (!uri) {
    if (err)
      g_error_free(err);
    return {};
  }
  std::string out(uri);
  g_free(uri);
  return out;
}

Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_buffer(GstBuffer* buf)
{
  GstMapInfo map;
  if (!gst_buffer_map(buf, &map, GST_MAP_READ))
    return {};
  Glib::RefPtr<Gdk::Pixbuf> out;
  try {
    auto loader = Gdk::PixbufLoader::create();
    loader->write(map.data, map.size);
    loader->close();
    if (auto pix = loader->get_pixbuf())
      out = pix->copy();
  } catch (const Glib::Error&) {
    out.reset();
  }
  gst_buffer_unmap(buf, &map);
  return out;
}

Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_tags(GstTagList* tags)
{
  const char* keys[] = {GST_TAG_IMAGE, GST_TAG_PREVIEW_IMAGE, nullptr};
  for (int k = 0; keys[k]; ++k) {
    const guint n = gst_tag_list_get_tag_size(tags, keys[k]);
    for (guint i = 0; i < n; ++i) {
      GstSample* sample = nullptr;
      if (!gst_tag_list_get_sample_index(tags, keys[k], i, &sample) || !sample)
        continue;
      GstBuffer* buf = gst_sample_get_buffer(sample);
      auto pix = buf ? pixbuf_from_buffer(buf) : Glib::RefPtr<Gdk::Pixbuf>{};
      gst_sample_unref(sample);
      if (pix)
        return pix;
    }
  }
  return {};
}

}  // namespace

Player::Player()
{
  playbin_ = gst_element_factory_make("playbin", "earblaster-playbin");
  if (!playbin_)
    return;

  GstElement* vsink = gst_element_factory_make("fakesink", "earblaster-vsink");
  if (vsink) {
    g_object_set(vsink, "sync", TRUE, nullptr);
    g_object_set(playbin_, "video-sink", vsink, nullptr);
  }
  g_object_set(playbin_, "volume", volume_, nullptr);

  GstBus* bus = gst_element_get_bus(playbin_);
  bus_watch_id_ = gst_bus_add_watch(bus, &Player::on_bus, this);
  gst_object_unref(bus);
}

Player::~Player()
{
  stop_position_timer();
  if (bus_watch_id_) {
    g_source_remove(bus_watch_id_);
    bus_watch_id_ = 0;
  }
  if (playbin_) {
    gst_element_set_state(playbin_, GST_STATE_NULL);
    gst_object_unref(playbin_);
    playbin_ = nullptr;
  }
}

bool Player::open(const std::string& path_or_uri)
{
  if (!playbin_) {
    signal_error_.emit("playbin is not available");
    return false;
  }
  const std::string uri = path_to_uri(path_or_uri);
  if (uri.empty()) {
    signal_error_.emit("Could not open file");
    return false;
  }

  gst_element_set_state(playbin_, GST_STATE_NULL);
  uri_ = uri;
  position_ = 0;
  duration_ = 0;
  g_object_set(playbin_, "uri", uri_.c_str(), "volume", volume_, nullptr);
  return true;
}

void Player::play()
{
  if (!playbin_ || uri_.empty())
    return;
  gst_element_set_state(playbin_, GST_STATE_PLAYING);
}

void Player::pause()
{
  if (!playbin_ || state_ != State::Playing)
    return;
  gst_element_set_state(playbin_, GST_STATE_PAUSED);
}

void Player::stop()
{
  if (!playbin_)
    return;
  stop_position_timer();
  gst_element_set_state(playbin_, GST_STATE_NULL);
  position_ = 0;
  set_state(State::Stopped);
  signal_position_changed_.emit(position_, duration_);
}

void Player::seek(gint64 ns)
{
  if (!playbin_ || uri_.empty() || ns < 0)
    return;
  gst_element_seek_simple(playbin_, GST_FORMAT_TIME,
                          static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                                    GST_SEEK_FLAG_KEY_UNIT),
                          ns);
}

void Player::set_volume(double volume)
{
  if (volume < 0.0)
    volume = 0.0;
  if (volume > 1.0)
    volume = 1.0;
  volume_ = volume;
  if (playbin_)
    g_object_set(playbin_, "volume", volume_, nullptr);
}

void Player::set_state(State state)
{
  if (state_ == state)
    return;
  state_ = state;
  if (state_ == State::Playing)
    start_position_timer();
  else
    stop_position_timer();
  signal_state_changed_.emit(state_);
}

void Player::start_position_timer()
{
  if (pos_timer_id_)
    return;
  pos_timer_id_ = g_timeout_add(250, &Player::on_position_timeout, this);
}

void Player::stop_position_timer()
{
  if (!pos_timer_id_)
    return;
  g_source_remove(pos_timer_id_);
  pos_timer_id_ = 0;
}

void Player::query_position()
{
  if (!playbin_)
    return;
  gint64 pos = 0;
  gint64 dur = 0;
  if (!gst_element_query_position(playbin_, GST_FORMAT_TIME, &pos))
    pos = position_;
  if (!gst_element_query_duration(playbin_, GST_FORMAT_TIME, &dur))
    dur = duration_;
  position_ = pos;
  duration_ = dur;
  signal_position_changed_.emit(position_, duration_);
}

gboolean Player::on_position_timeout(gpointer self)
{
  static_cast<Player*>(self)->query_position();
  return TRUE;
}

void Player::handle_tags(GstTagList* tags)
{
  auto pix = pixbuf_from_tags(tags);
  if (pix)
    signal_cover_.emit(pix);

  gchar* title = nullptr;
  gchar* artist = nullptr;
  gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
  gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist);
  if (title || artist) {
    signal_tags_.emit(title ? Glib::ustring(title) : Glib::ustring(),
                      artist ? Glib::ustring(artist) : Glib::ustring());
  }
  g_free(title);
  g_free(artist);
}

gboolean Player::on_bus(GstBus*, GstMessage* msg, gpointer self)
{
  auto* p = static_cast<Player*>(self);
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_STATE_CHANGED: {
      if (GST_MESSAGE_SRC(msg) != GST_OBJECT(p->playbin_))
        break;
      GstState old_st, new_st, pending;
      gst_message_parse_state_changed(msg, &old_st, &new_st, &pending);
      if (new_st == GST_STATE_PLAYING)
        p->set_state(State::Playing);
      else if (new_st == GST_STATE_PAUSED && p->state_ != State::Stopped)
        p->set_state(State::Paused);
      else if (new_st == GST_STATE_NULL || new_st == GST_STATE_READY)
        p->set_state(State::Stopped);
      if (new_st == GST_STATE_PLAYING || new_st == GST_STATE_PAUSED)
        p->query_position();
      break;
    }
    case GST_MESSAGE_EOS:
      p->signal_eos_.emit();
      break;
    case GST_MESSAGE_ERROR: {
      GError* err = nullptr;
      gst_message_parse_error(msg, &err, nullptr);
      Glib::ustring text = err ? err->message : "Playback error";
      if (err)
        g_error_free(err);
      p->signal_error_.emit(text);
      p->stop();
      break;
    }
    case GST_MESSAGE_TAG: {
      GstTagList* tags = nullptr;
      gst_message_parse_tag(msg, &tags);
      if (tags) {
        p->handle_tags(tags);
        gst_tag_list_unref(tags);
      }
      break;
    }
    default:
      break;
  }
  return TRUE;
}

}  // namespace earblaster
