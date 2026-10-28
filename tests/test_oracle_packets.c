// Invented packets exercise the recorder format without a guest executable.
#include "../runtime/oracle_packets.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void packet(char* line, const char* prefix, int count, const char* word) {
  strcpy(line, prefix);
  for (int i = 0; i < count; ++i) { strcat(line, " "); strcat(line, word); }
}
int main(void) {
  char line[4096];
  OraclePacket out;
  OraclePacketOrder order = {0, 0};
  packet(line, "1 1", 53, "ffffffff");
  CHECK(oracle_packet_parse(line, &out, &order));
  CHECK(out.frame == 1 && out.side == 1 && out.d[52] == UINT32_MAX);
  CHECK(!oracle_packet_parse(line, &out, &order)); // duplicate side in frame
  packet(line, "1 0", 53, "0");
  CHECK(oracle_packet_parse(line, &out, &order)); // retain caller's side order
  packet(line, "2 15", 53, "a");
  CHECK(oracle_packet_parse(line, &out, &order));
  const char* bad[] = {"1 0", "0 0", "3 16", "-3 0", "+3 0", "4294967296 0", "3 -1"};
  for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) {
    packet(line, bad[i], 53, "0"); CHECK(!oracle_packet_parse(line, &out, &order));
  }
  for (int count = 52; count <= 54; count += 2) {
    packet(line, "3 0", count, "0"); CHECK(!oracle_packet_parse(line, &out, &order));
  }
  const char* bad_word[] = {"-1", "+1", "100000000", "xyz", "1g"};
  for (unsigned i = 0; i < sizeof bad_word / sizeof *bad_word; ++i) {
    packet(line, "3 0", 53, bad_word[i]); CHECK(!oracle_packet_parse(line, &out, &order));
  }
  CHECK(order.frame == 2 && order.sides == (1u << 15)); // errors never change ordering state
  packet(line, "3 0", 53, "0"); strcat(line, "\r\n");
  CHECK(oracle_packet_parse(line, &out, &order));
  return failures ? 1 : 0;
}
