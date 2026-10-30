// Invented handles and ticks exercise the reference shim's public trace writer.
#include "../runtime/audio_trace.h"
#include <string.h>
int main(void) {
  FILE* file = tmpfile();
  if (!file) return 1;
  if (!rt_audio_write(file, 0, 17, 42, "play", 1) ||
      !rt_audio_write(file, 1, 18, 42, "stop", 0)) return 1;
  rewind(file);
  char line[512];
  int ok = fgets(line, sizeof line, file) && strstr(line, "\"seq\":0,\"tick\":17") &&
      strstr(line, "\"sound\":null,\"trigger\":\"unknown\"") &&
      fgets(line, sizeof line, file) && strstr(line, "\"seq\":1,\"tick\":18") &&
      strstr(line, "\"kind\":\"stop\"") && !fgets(line, sizeof line, file);
  fclose(file);
  return ok ? 0 : 1;
}
