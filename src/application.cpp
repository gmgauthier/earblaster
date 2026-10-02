/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"
#include "main_window.hpp"
#include "config.hpp"
#include "open_payload.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <glib.h>
#include <glibmm/miscutils.h>

#include <cerrno>
#include <cstring>
#include <iostream>

namespace earblaster {
namespace {

bool fill_unix_addr(sockaddr_un* addr, const std::string& path)
{
  if (path.size() >= sizeof(addr->sun_path))
    return false;
  std::memset(addr, 0, sizeof(*addr));
  addr->sun_family = AF_UNIX;
  std::memcpy(addr->sun_path, path.c_str(), path.size() + 1);
  return true;
}

std::string canonicalize_arg(const std::string& arg)
{
  auto file = Gio::File::create_for_commandline_arg(arg);
  const std::string path = file->get_path();
  return path.empty() ? arg : path;
}

}  // namespace

Glib::RefPtr<Application> Application::create()
{
  return Glib::RefPtr<Application>(new Application());
}

Application::Application()
    : Gtk::Application(APP_ID, Gio::APPLICATION_NON_UNIQUE)
{
}

Application::~Application()
{
  close_open_socket();
  if (lock_fd_ >= 0) {
    close(lock_fd_);
    lock_fd_ = -1;
  }
}

void Application::set_open_paths(std::vector<std::string> paths)
{
  open_paths_.clear();
  open_paths_.reserve(paths.size());
  for (const auto& p : paths)
    open_paths_.push_back(canonicalize_arg(p));
}

std::string Application::runtime_dir()
{
  const std::string dir = Glib::get_user_runtime_dir();
  g_mkdir_with_parents(dir.c_str(), 0700);
  return dir;
}

std::string Application::lock_path()
{
  return Glib::build_filename(runtime_dir(), "earblaster.lock");
}

std::string Application::socket_path()
{
  return Glib::build_filename(runtime_dir(), "earblaster.sock");
}

bool Application::take_instance_lock()
{
  lock_fd_ = ::open(lock_path().c_str(), O_CREAT | O_RDWR, 0600);
  if (lock_fd_ < 0)
    return true;
  if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
    close(lock_fd_);
    lock_fd_ = -1;
    return false;
  }
  return true;
}

void Application::listen_open_socket()
{
  const std::string path = socket_path();
  ::unlink(path.c_str());

  listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listen_fd_ < 0)
    return;

  sockaddr_un addr;
  if (!fill_unix_addr(&addr, path)) {
    close(listen_fd_);
    listen_fd_ = -1;
    return;
  }
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      ::listen(listen_fd_, 8) != 0) {
    close(listen_fd_);
    listen_fd_ = -1;
    return;
  }
  ::chmod(path.c_str(), 0600);

  listen_conn_ = Glib::signal_io().connect(sigc::mem_fun(*this, &Application::on_listen_io),
                                           listen_fd_, Glib::IO_IN | Glib::IO_HUP);
}

void Application::close_open_socket()
{
  listen_conn_.disconnect();
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
  ::unlink(socket_path().c_str());
}

bool Application::send_paths_to_primary() const
{
  const std::string path = socket_path();
  sockaddr_un addr;
  if (!fill_unix_addr(&addr, path))
    return false;

  for (int attempt = 0; attempt < 25; ++attempt) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
      return false;
    const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc != 0) {
      close(fd);
      g_usleep(20000);
      continue;
    }
    /* The whole list goes, or the primary sees no end marker and ignores it. */
    const bool sent = write_all(fd, encode_open_payload(open_paths_));
    close(fd);
    if (!sent)
      std::cerr << "earblaster: could not hand the file list to the running window\n";
    return sent;
  }
  return false;
}

bool Application::on_listen_io(Glib::IOCondition)
{
  if (listen_fd_ < 0)
    return false;

  const int cfd = ::accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC);
  if (cfd < 0)
    return true;

  std::string buf;
  const bool read_ok = read_all(cfd, buf);
  close(cfd);
  std::vector<std::string> paths;
  if (!read_ok || !decode_open_payload(buf, paths)) {
    /* Cut off or oversized: keep the current playlist rather than replace it
     * with part of a list. */
    std::cerr << "earblaster: ignored an incomplete file list from a second launch\n";
    paths.clear();
  }
  handle_open_paths(paths);
  return true;
}

void Application::handle_open_paths(const std::vector<std::string>& raw)
{
  std::vector<std::string> paths;
  paths.reserve(raw.size());
  for (const auto& p : raw)
    paths.push_back(canonicalize_arg(p));

  ensure_window();
  if (window_ == nullptr)
    return;
  if (!paths.empty())
    window_->open_paths(paths);
  window_->present();
}

void Application::ensure_window()
{
  if (window_ != nullptr)
    return;
  auto* win = new MainWindow();
  window_ = win;
  add_window(*win);
  win->signal_hide().connect([this, win]() {
    if (window_ == win)
      window_ = nullptr;
    delete win;
  });
}

void Application::on_startup()
{
  Gtk::Application::on_startup();
  lock_ok_ = take_instance_lock();
  if (lock_ok_)
    listen_open_socket();
}

void Application::on_activate()
{
  if (!lock_ok_) {
    if (!send_paths_to_primary())
      std::cerr << "earblaster: already running\n";
    quit();
    return;
  }

  ensure_window();
  if (window_ == nullptr)
    return;
  if (!open_paths_.empty())
    window_->open_paths(open_paths_);
  window_->present();
}

}  // namespace earblaster
