#include "settings.h"
#include "dbg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Settings g_settings = {0, 0, 0, 0, 1};
static char path[1100];

void settings_init(const char* exe_path) {
  snprintf(path, sizeof path, "%s", exe_path);
  char* sl = strrchr(path, '/');
  if (sl) *sl = 0; else strcpy(path, ".");
  strncat(path, "/aztec_settings.ini", sizeof path - strlen(path) - 1);
  FILE* f = fopen(path, "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof line, f)) {
      int v, w, h;
      if (sscanf(line, "resolution=%dx%d", &w, &h) == 2) { g_settings.res_w = w; g_settings.res_h = h; }
      else if (sscanf(line, "window_mode=%d", &v) == 1) g_settings.window_mode = v;
      else if (sscanf(line, "smooth=%d", &v) == 1) g_settings.smooth = v;
      else if (sscanf(line, "video=%d", &v) == 1) g_settings.video = v;
    }
    fclose(f);
  }
  DBG("settings", "loaded %s: res=%dx%d window_mode=%d smooth=%d video=%d (file %s)", path, g_settings.res_w, g_settings.res_h, g_settings.window_mode, g_settings.smooth, g_settings.video, f ? "found" : "MISSING, defaults");
  int w, h;
  const char* r = getenv("RT_RESOLUTION");
  if (r && sscanf(r, "%dx%d", &w, &h) == 2) { g_settings.res_w = w; g_settings.res_h = h; }
  if (getenv("RT_NO_VIDEO")) g_settings.video = 0;
  if (getenv("RT_FULLSCREEN")) g_settings.window_mode = 2;
}

void settings_save(void) {
  DBG("settings", "saving %s: res=%dx%d window_mode=%d smooth=%d video=%d", path, g_settings.res_w, g_settings.res_h, g_settings.window_mode, g_settings.smooth, g_settings.video);
  FILE* f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "resolution=%dx%d\nwindow_mode=%d\nsmooth=%d\nvideo=%d\n", g_settings.res_w, g_settings.res_h,
          g_settings.window_mode, g_settings.smooth, g_settings.video);
  fclose(f);
}
