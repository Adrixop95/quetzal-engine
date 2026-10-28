// Independent parser for the public RT_CMD_LOG text format. No guest-code dependency.
#ifndef AZTEC_ORACLE_PACKETS_H
#define AZTEC_ORACLE_PACKETS_H
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define CMD_DWORDS 53
typedef struct { uint32_t frame, side; uint32_t d[CMD_DWORDS]; } OraclePacket;
typedef struct { uint32_t frame, sides; } OraclePacketOrder;

static int oracle_number(const char** cursor, int base, uint32_t* out) {
  const char* p = *cursor;
  char* end;
  while (isspace((unsigned char)*p)) ++p;
  if (!(base == 10 ? isdigit((unsigned char)*p) : isxdigit((unsigned char)*p))) return 0;
  errno = 0;
  unsigned long long value = strtoull(p, &end, base);
  if (errno || value > UINT32_MAX || (*end && !isspace((unsigned char)*end))) return 0;
  *cursor = end; *out = (uint32_t)value;
  return 1;
}

static int oracle_packet_parse(const char* line, OraclePacket* out, OraclePacketOrder* order) {
  OraclePacket packet;
  if (!oracle_number(&line, 10, &packet.frame) || !oracle_number(&line, 10, &packet.side)) return 0;
  for (int i = 0; i < CMD_DWORDS; ++i) if (!oracle_number(&line, 16, &packet.d[i])) return 0;
  while (isspace((unsigned char)*line)) ++line;
  if (*line || !packet.frame || packet.frame < order->frame || packet.side >= 16) return 0;
  uint32_t sides = packet.frame == order->frame ? order->sides : 0;
  if (sides & (1u << packet.side)) return 0;
  order->frame = packet.frame; order->sides = sides | (1u << packet.side);
  *out = packet;
  return 1;
}
#endif
