#include "core/savegame.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <array>
using namespace aztec;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"Failed at %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

using Bytes = std::vector<uint8_t>;
static void put(Bytes& b, size_t p, uint32_t v) {
  CHECK(p + 4 <= b.size());
  for (unsigned i=0;i<4;++i) b[p+i]=uint8_t(v>>(8*i));
}
static void word(Bytes& b, uint32_t v) { size_t p=b.size(); b.resize(p+4); put(b,p,v); }
static void zeros(Bytes& b, size_t n) { b.resize(b.size()+n); }
struct Fixture {
  std::array<Bytes,10> sections;
  std::array<uint32_t,10> counts{};
  Bytes finish() const {
    Bytes b(0x158+28); put(b,0x158,0xfffffff0); put(b,0x170,123);
    for(size_t i=0;i<10;++i) { word(b,counts[i]); b.insert(b.end(),sections[i].begin(),sections[i].end()); }
    zeros(b,4520); word(b,7199); word(b,8); return b;
  }
};
static void reject(const Bytes& b) {
  SaveGame g; g.bytes={1}; SaveError e; CHECK(!parse_savegame(b,g,&e));
  CHECK(g.bytes.empty() && g.sections.empty()); CHECK(!e.message.empty()); CHECK(e.offset<=b.size());
}
static void roundtrip(const Bytes& bytes) {
  SaveGame first, second; SaveError error; Bytes emitted{1};
  CHECK(parse_savegame(bytes, first, &error));
  if (!serialize_savegame(first, emitted, &error)) {
    std::fprintf(stderr,"Round trip of %zu bytes failed at %zu: %s\n",bytes.size(),error.offset,error.message.c_str());
    CHECK(false);
  }
  CHECK(emitted == bytes && error.message.empty());
  CHECK(parse_savegame(emitted, second, &error));
  CHECK(serialize_savegame(second, emitted, &error) && emitted == bytes);
}
static void profiles() {
  std::string error;
  PlayerFile p; p.name = std::string(66, 'A'); p.flag = 7; p.const_one = 16;
  p.reserved = 0x12345678;
  p.options.gamma = 1.125; p.options.speech_style = 3;
  p.options.sfx_volume = 11; p.options.music_volume = 22; p.options.speech_volume = 33;
  p.options.acknowledgement = 0; p.options.frequency = 44100; p.options.display_mode = 2;
  p.options.scroll_speed = 31; p.options.game_speed = 79;
  p.options.view_all = 1; p.options.b9 = 2; p.options.default_provider = 3;
  p.raw.resize(300);
  for (size_t i = 0; i < p.raw.size(); ++i) p.raw[i] = uint8_t(i * 17 + 13);
  auto bytes = serialize_player_file(p, &error);
  CHECK(bytes.size() == 300 && error.empty());
  CHECK(bytes[0] == 7 && bytes[67] == 0 && bytes[68] == 16);
  CHECK(bytes[72] == 0x78 && bytes[75] == 0x12 && bytes[0x4c] == p.raw[0x4c]);
  CHECK(bytes[0x4c + 0xa0] == 0x44 && bytes[0x4c + 0xa1] == 0xac);
  CHECK(bytes[0x4c + 0xb6] == 0xf2 && bytes[0x4c + 0xb7] == 0x3f);
  PlayerFile decoded;
  CHECK(parse_player_file(bytes, decoded, &error) && error.empty());
  CHECK(decoded.name == p.name && decoded.const_one == 16 && decoded.reserved == p.reserved);
  CHECK(decoded.options.gamma == 1.125 && decoded.options.speech_style == 3);
  CHECK(serialize_player_file(decoded) == bytes);
  CHECK(parse_player_file(decoded.raw, decoded)); // Aliased input remains valid.
  CHECK(serialize_player_file(decoded) == bytes);
  for (size_t n = 0; n < bytes.size(); ++n) {
    CHECK(!parse_player_file(Bytes(bytes.begin(), bytes.begin() + n), decoded, &error));
    CHECK(!error.empty() && decoded.raw.empty() && decoded.name.empty());
  }
  auto bad = bytes; bad.push_back(0); CHECK(!parse_player_file(bad, decoded, &error));
  bad = bytes; std::fill(bad.begin() + 1, bad.begin() + 0x44, 'X');
  CHECK(!parse_player_file(bad, decoded, &error) && !error.empty());
  auto invalid = p; invalid.name += 'X'; CHECK(serialize_player_file(invalid, &error).empty() && !error.empty());
  invalid.name = std::string("A\0B", 3); CHECK(serialize_player_file(invalid, &error).empty());
  invalid = p; invalid.raw.resize(299); CHECK(serialize_player_file(invalid, &error).empty());
  TempDir dir; const auto path = (dir.path() / "synthetic.plr").string();
  CHECK(write_player_file(path, p, &error) && error.empty());
  CHECK(load_player_file(path, decoded, &error) && serialize_player_file(decoded) == bytes);
  p.name = "Changed"; p.const_one = 3;
  CHECK(write_player_file(path, p, &error));
  CHECK(load_player_file(path, decoded) && decoded.name == "Changed" && decoded.const_one == 3);
  CHECK(!write_player_file(path, invalid, &error) && !error.empty());
  CHECK(load_player_file(path, decoded) && decoded.name == "Changed");
  CHECK(!load_player_file((dir.path() / "missing").string(), decoded, &error));
  CHECK(decoded.raw.empty() && !error.empty());
  CHECK(!write_player_file((dir.path() / "missing" / "x.plr").string(), p, &error));
  CHECK(!error.empty());
}
static void writer(const Bytes& bytes) {
  SaveGame game; SaveError error; CHECK(parse_savegame(bytes, game));
  // Exercise retained opaque bytes, unaligned scalars and header padding.
  for (auto& section : game.sections) for (auto& record : section.records)
    for (auto& field : record.fields) if (field.kind == SaveValue::bytes)
      for (size_t i = 0; i < field.size; ++i) game.bytes[field.offset + i] = uint8_t(i * 19 + 7);
  game.header.raw[0x107] = 0xa7; game.header.raw[0x147] = 0xb8;
  game.header.map_path = "Maps/synthetic.mp"; game.header.description = "Synthetic slot";
  game.header.zero0 = 1; game.header.zero1 = 15;
  for (size_t i = 0; i < 8; ++i) game.header.time[i] = uint16_t(0x1234 + i);
  game.view_x = -123; game.view_y = 456; game.globals[4] = 0xabcdef01;
  auto& unit = game.sections[4].records[0];
  for (auto& field : unit.fields) if (field.memory_offset == 0x38 && field.scope == "object") field.value = 5432;
  Bytes emitted; CHECK(serialize_savegame(game, emitted, &error));
  CHECK(emitted[0x107] == 0xa7 && emitted[0x147] == 0xb8);
  CHECK(emitted[0x148] == 0x34 && emitted[0x149] == 0x12);
  SaveGame decoded; CHECK(parse_savegame(emitted, decoded));
  CHECK(decoded.header.map_path == game.header.map_path && decoded.header.zero1 == 15);
  CHECK(decoded.header.description == game.header.description && decoded.header.zero0 == 1);
  CHECK(decoded.view_x == -123 && decoded.view_y == 456 && decoded.globals[4] == 0xabcdef01);
  CHECK(decoded.sections[4].records[0].field(0x38)->value == 5432);
  roundtrip(emitted);
  TempDir dir; const auto path = (dir.path() / "single0.sav").string();
  CHECK(write_savegame(path, game, &error) && error.message.empty());
  CHECK(load_savegame(path, decoded) && decoded.bytes == emitted);
  game.header.description = "Replacement";
  CHECK(write_savegame(path, game, &error));
  CHECK(load_savegame(path, decoded) && decoded.header.description == "Replacement");
  auto invalid = game; invalid.header.description.resize(64, 'X');
  CHECK(!write_savegame(path, invalid, &error) && !error.message.empty());
  CHECK(load_savegame(path, decoded) && decoded.header.description == "Replacement");
  CHECK(!write_savegame((dir.path() / "absent" / "single0.sav").string(), game, &error));
  CHECK(!error.message.empty());
  auto reject_model = [&](const SaveGame& model) {
    Bytes output{1}; CHECK(!serialize_savegame(model, output, &error));
    CHECK(output.empty() && !error.message.empty());
  };
  invalid = game; invalid.sections.pop_back(); reject_model(invalid);
  invalid = game; std::swap(invalid.sections[0], invalid.sections[1]); reject_model(invalid);
  invalid = game; invalid.sections[10].records.clear(); reject_model(invalid);
  invalid = game; invalid.sections[4].records[0].fields[0].size = 1; reject_model(invalid);
  invalid = game; invalid.bytes.clear(); reject_model(invalid);
  invalid = game;
  for (auto& field : invalid.sections[1].records[0].fields)
    if (field.kind == SaveValue::bytes) { field.offset = SIZE_MAX; break; }
  reject_model(invalid);
  invalid = game;
  invalid.sections[1].records[0].fields[0].size = SIZE_MAX; reject_model(invalid);
  invalid = game; invalid.sections[4].records.push_back(invalid.sections[4].records[0]); reject_model(invalid);
  invalid = game;
  for (auto& field : invalid.sections[4].records[0].fields)
    if (field.kind == SaveValue::unit_ref) { field.value = 0x7fffffff; break; }
  reject_model(invalid);
  invalid = game; invalid.sections[4].records[0].fields[1].value = 90; reject_model(invalid);
  // Removing an unreferenced record recomputes the outer count and byte ranges.
  game.sections[5].records.pop_back();
  CHECK(serialize_savegame(game, emitted, &error) && parse_savegame(emitted, decoded));
  CHECK(decoded.sections[5].records.size() == game.sections[5].records.size());
}
static void synthetic() {
  SaveGame g; SaveError error; Fixture f; Bytes empty=f.finish();
  CHECK(parse_savegame(empty,g,&error)); CHECK(error.message.empty());
  roundtrip(empty);
  CHECK(g.view_x==-16 && g.globals[4]==123 && g.sections.size()==12);
  CHECK(g.section("daylight")->records[0].fields[0].value==7199);
  CHECK(parse_savegame(g.bytes,g,&error)); CHECK(g.bytes==empty);
  CHECK(!g.section("missing")); CHECK(!g.section("ui")->records[0].field(123));
  for(size_t n=0;n<empty.size();++n) reject(Bytes(empty.begin(),empty.begin()+n));
  auto bad=empty; bad.push_back(0); reject(bad);
  for(size_t p=372;p<412;p+=4) for(uint32_t v:{16385u,0x80000000u,0xffffffffu}) { bad=empty; put(bad,p,v); reject(bad); }
  CHECK(!load_savegame("/nonexistent/aztec-save-test",g,&error)); CHECK(g.sections.empty());

  // Fixed minimum payload lengths, including the five-word inactive animation.
  constexpr unsigned unit_words[]={45,50,39,40,43,42,51,43,30,41,32,25};
  constexpr unsigned representatives[]={0,30,10,7,83,33,3,17,84,87,88,89};
  for(unsigned kind=0;kind<12;++kind) CHECK(save_unit_kind(representatives[kind])==int(kind));
  for(unsigned type=0;type<90;++type) {
    unsigned k=unsigned(save_unit_kind(type)); auto& b=f.sections[4]; ++f.counts[4];
    word(b,type+1); word(b,type); size_t p=b.size(); zeros(b,4*unit_words[k]);
    put(b,p+6*4,0x12345678); put(b,p+10*4,6000); put(b,p+11*4,7000); put(b,p+16*4,type);
  }
  constexpr unsigned weapon_words[]={26,27,11,24,10,10,10,18,62,11,10,16,11,11,14};
  for(unsigned type=0;type<40;++type) {
    auto& b=f.sections[5]; ++f.counts[5]; word(b,type); zeros(b,4*weapon_words[save_weapon_kind(type)]);
  }
  CHECK(save_unit_kind(58)==6 && save_unit_kind(68)==2 && save_unit_kind(86)==7 && save_unit_kind(82)==0);
  CHECK(save_unit_kind(90)==-1 && save_unit_kind(UINT32_MAX)==-1);
  CHECK(save_weapon_kind(40)==-1 && save_weapon_kind(UINT32_MAX)==-1);
  // Town/Side include raw blocks, so the next section need not be aligned.
  f.counts[1]=1; word(f.sections[1],0x80000001); zeros(f.sections[1],5624);
  f.counts[2]=1; zeros(f.sections[2],1657);
  f.counts[0]=1; word(f.sections[0],0x80000009); zeros(f.sections[0],14*4);
  f.counts[6]=1; word(f.sections[6],9); zeros(f.sections[6],36);
  f.counts[7]=1; word(f.sections[7],9); zeros(f.sections[7],296);
  for(unsigned k:{0u,2u,3u,4u,5u}) {
    ++f.counts[3]; word(f.sections[3],k+1); word(f.sections[3],k);
    zeros(f.sections[3],236+(k==2 ? 16 : k==3 ? 4 : k==4 ? 8 : 0));
  }
  for(unsigned k=0;k<5;++k) {
    ++f.counts[8]; word(f.sections[8],k+1); word(f.sections[8],k);
    zeros(f.sections[8],k==0 ? 8 : k==1 ? 20 : k==2 ? 24 : k==3 ? 12 : 8);
  }
  Bytes all=f.finish(); CHECK(parse_savegame(all,g,&error)); CHECK(g.bytes==all);
  roundtrip(all); writer(all);
  CHECK(validate_savegame_references(g, &error));
  CHECK(g.section("units")->records.size()==90 && g.section("weapons")->records.size()==40);
  for(const auto& r:g.section("units")->records) {
    CHECK(r.field(0x38)->value==6000 && r.field(0x3c)->value==7000);
    CHECK(r.field(0x50)->value==r.type && r.field(0x28)->value==0x12345678);
    CHECK(r.size==8+unit_words[r.kind]*4);
  }
  for(const auto& sec:g.sections) for(const auto& r:sec.records) {
    size_t p=r.offset;
    for(const auto& field:r.fields) { CHECK(field.offset==p); p+=field.size; CHECK(p<=all.size()); }
    CHECK(p==r.offset+r.size);
    if(r.size) reject(Bytes(all.begin(),all.begin()+r.offset+r.size-1));
  }
  bad=all; put(bad,g.section("units")->records[0].offset+4,90); reject(bad);
  bad=all; put(bad,g.section("weapons")->records[0].offset,40); reject(bad);
  bad=all; put(bad,g.section("tactics")->records[0].offset+4,1); reject(bad);
  bad=all; put(bad,g.section("scripts")->records[0].offset+4,5); reject(bad);
  bad=all; std::fill(bad.begin()+8,bad.begin()+0x108,'X'); reject(bad);
  bad=all; std::fill(bad.begin()+0x108,bad.begin()+0x148,'X'); reject(bad);
  SaveHeader header; std::string header_error;
  CHECK(parse_save_header(all,header,&header_error));
  CHECK(parse_save_header(header.raw,header,&header_error));
  CHECK(serialize_save_header(header)==Bytes(all.begin(),all.begin()+0x158));
  for(size_t n=0;n<0x158;++n) {
    CHECK(!parse_save_header(Bytes(all.begin(),all.begin()+n),header,&header_error));
    CHECK(header.raw.empty() && !header_error.empty());
  }
  header.map_path.resize(256,'X'); CHECK(serialize_save_header(header,&header_error).empty());
  header={}; header.description=std::string("A\0B",3); CHECK(serialize_save_header(header,&header_error).empty());
  header={}; header.body_offset=0; CHECK(serialize_save_header(header,&header_error).empty());
  header={}; header.raw.resize(10); CHECK(serialize_save_header(header,&header_error).empty());
  CHECK(!load_save_header("/nonexistent/aztec-save-header-test",header,&header_error));
  CHECK(header.raw.empty() && !header_error.empty());

  // Active Marine animation, with frame indices and byte members widened to words.
  Fixture active; active.counts[4]=1; auto& u=active.sections[4]; word(u,1); word(u,0); zeros(u,40*4);
  word(u,1000); word(u,1); word(u,3); word(u,9); word(u,1); word(u,7); word(u,1);
  CHECK(parse_savegame(active.finish(),g,&error));
  roundtrip(active.finish());
  const auto& fields=g.section("units")->records[0].fields;
  CHECK(fields[fields.size()-5].value==3 && fields[fields.size()-4].value==9);

  // A timer sample array and a scripted route exercise counted payloads.
  Fixture nested; nested.counts[8]=2; auto& b=nested.sections[8];
  for(uint32_t v:{1u,2u,10u,12u,1u,1u,2u,1u,100u,200u,2u,4u,5u,2u,10u,20u,30u,40u}) word(b,v);
  CHECK(parse_savegame(nested.finish(),g,&error));
  roundtrip(nested.finish());
  CHECK(g.section("scripts")->records[0].fields.back().size==8);
  bad=nested.finish(); put(bad,372+8*4+4+6*4,UINT32_MAX); reject(bad);

  // Building optional harvest objects and Defence's optional second animation.
  Fixture optional; optional.counts[4]=2; auto& ob=optional.sections[4];
  word(ob,1); word(ob,10); zeros(ob,37*4); word(ob,1);
  word(ob,5); word(ob,6); word(ob,7); word(ob,1); word(ob,8); word(ob,9); word(ob,10);
  word(ob,2); word(ob,17); zeros(ob,42*4); word(ob,1); zeros(ob,5*4);
  optional.counts[8]=2;
  for(uint32_t v:{5u,0u,0u,0u,8u,0u,0u,0u}) word(optional.sections[8],v);
  CHECK(parse_savegame(optional.finish(),g,&error));
  roundtrip(optional.finish());
  CHECK(g.section("units")->records[0].field(4,"object.harvest112")->value==5);
  CHECK(g.section("units")->records[1].field(0,"object.animation116"));

  Fixture matrix; matrix.counts[3]=1; auto& t=matrix.sections[3]; word(t,1); word(t,0);
  size_t base=t.size(); zeros(t,212); put(t,base+48,1); put(t,base+108,2); put(t,base+112,1);
  word(t,1); zeros(t,44); zeros(t,12); // member and point triple
  word(t,2); word(t,1); word(t,0x80000001); // target references
  word(t,1); word(t,1); word(t,20); word(t,30); // route presence and row
  word(t,1); word(t,40); word(t,2); word(t,50); // columns
  word(t,60); word(t,70); // one by two matrix
  word(t,1); word(t,80); word(t,90); word(t,0); word(t,1); word(t,1);
  matrix.counts[1]=1; word(matrix.sections[1],0x80000001); zeros(matrix.sections[1],5624);
  matrix.counts[4]=2;
  for(uint32_t id:{1u,2u}) { word(matrix.sections[4],id); word(matrix.sections[4],0); zeros(matrix.sections[4],45*4); }
  CHECK(parse_savegame(matrix.finish(),g,&error));
  roundtrip(matrix.finish());
  const auto& mf=g.section("tactics")->records[0].fields;
  CHECK(std::count_if(mf.begin(),mf.end(),[](const SaveField& v){return v.name=="route_matrix" && v.size==8;})==1);
  bad=matrix.finish(); put(bad,g.section("tactics")->records[0].offset+8+108,UINT32_MAX); reject(bad);
  Fixture fog; fog.counts[2]=1; zeros(fog.sections[2],1657);
  bad=fog.finish(); put(bad,372+8+4+1645,UINT32_MAX); put(bad,372+8+4+1649,UINT32_MAX); reject(bad);
  // Nonempty opaque packets, explorers, camera entries and unaligned fog bytes.
  Fixture opaque; opaque.counts[2]=1; auto& side=opaque.sections[2]; zeros(side,1641);
  word(side,1); word(side,1); word(side,0); zeros(side,0x168);
  word(side,2); word(side,3); side.insert(side.end(),{0,32,64,96,128,152});
  word(side,1); zeros(side,20);
  opaque.counts[8]=1; auto& trigger=opaque.sections[8];
  for(uint32_t v:{7u,1u,99u,1u,1u,1u}) word(trigger,v);
  zeros(trigger,0x408); word(trigger,1); zeros(trigger,0x408);
  CHECK(parse_savegame(opaque.finish(),g,&error));
  for(const auto& section:g.sections) for(const auto& record:section.records)
    for(const auto& field:record.fields) if(field.kind==SaveValue::bytes && field.name!="fog")
      for(size_t i=0;i<field.size;++i) g.bytes[field.offset+i]=uint8_t(i*7+3);
  Bytes opaque_bytes; CHECK(serialize_savegame(g,opaque_bytes,&error)); roundtrip(opaque_bytes);
  // The original queue has 0x800 bytes for eight-byte entries.
  Fixture queue; queue.counts[9] = 1;
  for (uint32_t v : {1u, 0u, 0u, 256u, 0u}) word(queue.sections[9], v);
  zeros(queue.sections[9], 256 * 8);
  CHECK(parse_savegame(queue.finish(), g, &error));
  roundtrip(queue.finish());
  CHECK(validate_savegame_references(g, &error));
  put(queue.sections[9], 12, 257); zeros(queue.sections[9], 8); reject(queue.finish());
  std::puts("Synthetic save-game layouts and malformed inputs passed");
}
int main(int argc, char** argv) {
  profiles();
  synthetic();
  for(int i=1;i<argc;++i) {
    SaveGame game; SaveError error;
    if(!load_savegame(argv[i],game,&error)) { std::fprintf(stderr,"%s at %zu: %s\n",argv[i],error.offset,error.message.c_str()); return 1; }
    std::printf("%s: %zu bytes tick=%u\n",argv[i],game.bytes.size(),game.globals[4]);
    for(const auto& s:game.sections) std::printf("  %s: offset=%zu bytes=%zu records=%zu\n",s.name.c_str(),s.offset,s.size,s.records.size());
  }
  return 0;
}
