#include "engine/render/font.hpp"

#include <cctype>
#include <vector>

namespace aztec::engine {

struct Glyph { char ch; const char* bits; };  // 15 chars, row-major, '1' = set pixel
static const Glyph kGlyphs[] = {
    {'0', "111101101101111"},
    {'1', "010110010010111"},
    {'2', "111001111100111"},
    {'3', "111001111001111"},
    {'4', "101101111001001"},
    {'5', "111100111001111"},
    {'6', "111100111101111"},
    {'7', "111001001001001"},
    {'8', "111101111101111"},
    {'9', "111101111001111"},
    {'A', "010101111101101"},
    {'B', "110101110101110"},
    {'C', "011100100100011"},
    {'D', "110101101101110"},
    {'E', "111100110100111"},
    {'F', "111100110100100"},
    {'G', "011100101101011"},
    {'H', "101101111101101"},
    {'I', "111010010010111"},
    {'J', "001001001101010"},
    {'K', "101101110101101"},
    {'L', "100100100100111"},
    {'M', "101111111101101"},
    {'N', "110101101101101"},
    {'O', "010101101101010"},
    {'P', "110101110100100"},
    {'Q', "010101101111011"},
    {'R', "110101110101101"},
    {'S', "011100010001110"},
    {'T', "111010010010010"},
    {'U', "101101101101111"},
    {'V', "101101101101010"},
    {'W', "101101111111101"},
    {'X', "101101010101101"},
    {'Y', "101101010010010"},
    {'Z', "111001010100111"},
    {'.', "000000000000010"},
    {':', "000010000010000"},
    {',', "000000000010100"},
    {'-', "000000111000000"},
    {'+', "000010111010000"},
    {'/', "001001010100100"},
    {'%', "101001010100101"},
    {'(', "010100100100010"},
    {')', "010001001001010"},
    {'=', "000111000111000"},
    {'_', "000000000000111"},
    {'#', "101111101111101"},
    {'[', "110100100100110"},
    {']', "011001001001011"},
    {'?', "111001010000010"},
    {'!', "010010010000010"},
    {'*', "101010111010101"},
};

// Texture layout: 96 cells of 4x5 px, char code c at column c - 32.
bool Font::init(SDL_Renderer* r) {
  const int cells = 96;
  std::vector<uint32_t> px(size_t(cells) * 4 * 5, 0);
  for (const Glyph& g : kGlyphs) {
    int cx = (g.ch - 32) * 4;
    for (int i = 0; i < 15; i++)
      if (g.bits[i] == '1') px[size_t(i / 3) * cells * 4 + cx + i % 3] = 0xFFFFFFFFu;
  }
  tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, cells * 4, 5);
  if (!tex_) return false;
  SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
  SDL_UpdateTexture(tex_, nullptr, px.data(), cells * 4 * 4);
  return true;
}

Font::~Font() { if (tex_) SDL_DestroyTexture(tex_); }

void Font::draw(SDL_Renderer* r, int x, int y, const std::string& s, int scale, SDL_Color c, bool shadow) const {
  if (!tex_) return;
  for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
    SDL_Color col = pass == 0 ? SDL_Color{0, 0, 0, 200} : c;
    SDL_SetTextureColorMod(tex_, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(tex_, col.a);
    int ox = pass == 0 ? scale : 0, oy = pass == 0 ? scale : 0;
    int cx = x;
    for (unsigned char ch : s) {
      ch = (unsigned char)std::toupper(ch);
      if (ch >= 32 && ch < 128 && ch != ' ') {
        SDL_Rect src{(ch - 32) * 4, 0, 3, 5}, dst{cx + ox, y + oy, 3 * scale, 5 * scale};
        SDL_RenderCopy(r, tex_, &src, &dst);
      }
      cx += 4 * scale;
    }
  }
}

}  // namespace aztec::engine
