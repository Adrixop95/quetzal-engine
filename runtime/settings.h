// Launcher/overlay settings, persisted in <game dir>/aztec_settings.ini
#pragma once
typedef struct {
  int res_w, res_h;     // 0 = game default (800x600)
  int window_mode;      // 0 windowed, 1 borderless, 2 fullscreen (desktop)
  int smooth;           // 0 nearest-neighbour, 1 linear scaling
  int video;            // play movies
} Settings;
extern Settings g_settings;
void settings_init(const char* exe_path);   // load from file, then apply RT_* environment overrides
void settings_save(void);
