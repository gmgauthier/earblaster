/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"

#include <glib.h>
#include <gst/gst.h>

int main(int argc, char* argv[])
{
  if (g_getenv("GDK_BACKEND") == nullptr)
    g_setenv("GDK_BACKEND", "x11", FALSE);
  g_set_prgname("earblaster");
  gst_init(&argc, &argv);

  return earblaster::Application::create()->run(argc, argv);
}
