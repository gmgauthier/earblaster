/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"

#include <glib.h>
#include <gst/gst.h>

#include <string>

namespace {

/* AppImage ships libgstreamer *and* plugins. Mixing those with the host's
 * plugins (Arch vs Debian) loads e.g. host libgstplayback.so against the
 * bundled libgstreamer and fails (undefined symbol). Use only APPDIR
 * plugins and a private registry, never ~/.cache from a native GST. */
void setup_gst_plugin_path()
{
  const char* appdir = g_getenv("APPDIR");
  if (appdir == nullptr || appdir[0] == '\0')
    return;

  std::string path;
  const char* bundled[] = {
      "/usr/lib/gstreamer-1.0",
      "/usr/lib/x86_64-linux-gnu/gstreamer-1.0",
      "/usr/lib/aarch64-linux-gnu/gstreamer-1.0",
      nullptr};
  for (int i = 0; bundled[i]; ++i) {
    const std::string cand = std::string(appdir) + bundled[i];
    if (g_file_test(cand.c_str(), G_FILE_TEST_IS_DIR)) {
      path = cand;
      break;
    }
  }
  if (path.empty())
    return;

  g_unsetenv("GST_PLUGIN_PATH");
  g_unsetenv("GST_PLUGIN_SYSTEM_PATH");
  g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", path.c_str(), TRUE);
  g_setenv("GST_PLUGIN_PATH_1_0", path.c_str(), TRUE);

  const std::string scanner =
      std::string(appdir) + "/usr/lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner";
  if (g_file_test(scanner.c_str(), G_FILE_TEST_IS_EXECUTABLE))
    g_setenv("GST_PLUGIN_SCANNER_1_0", scanner.c_str(), TRUE);

  const std::string cache =
      std::string(g_get_user_cache_dir()) + "/gstreamer-1.0";
  g_mkdir_with_parents(cache.c_str(), 0700);
  const std::string registry = cache + "/earblaster-appimage.bin";
  g_setenv("GST_REGISTRY_1_0", registry.c_str(), TRUE);
}

}  // namespace

int main(int argc, char* argv[])
{
  if (g_getenv("GDK_BACKEND") == nullptr)
    g_setenv("GDK_BACKEND", "x11", FALSE);
  g_set_prgname("earblaster");
  setup_gst_plugin_path();
  gst_init(&argc, &argv);

  return earblaster::Application::create()->run(argc, argv);
}
