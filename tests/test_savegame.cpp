#include "core/savegame.hpp"
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
static void synthetic() {
  SaveGame g; SaveError error; Fixture f; Bytes empty=f.finish();
  CHECK(parse_savegame(empty,g,&error)); CHECK(error.message.empty());
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
  for(unsigned k:{0u,2u,3u,4u,5u}) {
    ++f.counts[3]; word(f.sections[3],k+1); word(f.sections[3],k);
    zeros(f.sections[3],236+(k==2 ? 16 : k==3 ? 4 : k==4 ? 8 : 0));
  }
  for(unsigned k=0;k<5;++k) {
    ++f.counts[8]; word(f.sections[8],k+1); word(f.sections[8],k);
    zeros(f.sections[8],k==0 ? 8 : k==1 ? 20 : k==2 ? 24 : k==3 ? 12 : 8);
  }
  Bytes all=f.finish(); CHECK(parse_savegame(all,g,&error)); CHECK(g.bytes==all);
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

  // Active Marine animation, with frame indices and byte members widened to words.
  Fixture active; active.counts[4]=1; auto& u=active.sections[4]; word(u,1); word(u,0); zeros(u,40*4);
  word(u,1000); word(u,1); word(u,3); word(u,9); word(u,1); word(u,7); word(u,1);
  CHECK(parse_savegame(active.finish(),g,&error));
  const auto& fields=g.section("units")->records[0].fields;
  CHECK(fields[fields.size()-5].value==3 && fields[fields.size()-4].value==9);

  // A timer sample array and a scripted route exercise counted payloads.
  Fixture nested; nested.counts[8]=2; auto& b=nested.sections[8];
  for(uint32_t v:{1u,2u,10u,12u,1u,1u,2u,1u,100u,200u,2u,4u,5u,2u,10u,20u,30u,40u}) word(b,v);
  CHECK(parse_savegame(nested.finish(),g,&error));
  CHECK(g.section("scripts")->records[0].fields.back().size==8);
  bad=nested.finish(); put(bad,372+8*4+4+6*4,UINT32_MAX); reject(bad);

  // Building optional harvest objects and Defence's optional second animation.
  Fixture optional; optional.counts[4]=2; auto& ob=optional.sections[4];
  word(ob,1); word(ob,10); zeros(ob,37*4); word(ob,1);
  word(ob,5); word(ob,6); word(ob,7); word(ob,1); word(ob,8); word(ob,9); word(ob,10);
  word(ob,2); word(ob,17); zeros(ob,42*4); word(ob,1); zeros(ob,5*4);
  CHECK(parse_savegame(optional.finish(),g,&error));
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
  CHECK(parse_savegame(matrix.finish(),g,&error));
  const auto& mf=g.section("tactics")->records[0].fields;
  CHECK(std::count_if(mf.begin(),mf.end(),[](const SaveField& v){return v.name=="route_matrix" && v.size==8;})==1);
  bad=matrix.finish(); put(bad,g.section("tactics")->records[0].offset+8+108,UINT32_MAX); reject(bad);
  Fixture fog; fog.counts[2]=1; zeros(fog.sections[2],1657);
  bad=fog.finish(); put(bad,372+8+4+1645,UINT32_MAX); put(bad,372+8+4+1649,UINT32_MAX); reject(bad);
  std::puts("Synthetic save-game layouts and malformed inputs passed");
}
int main(int argc, char** argv) {
  synthetic();
  for(int i=1;i<argc;++i) {
    SaveGame game; SaveError error;
    if(!load_savegame(argv[i],game,&error)) { std::fprintf(stderr,"%s at %zu: %s\n",argv[i],error.offset,error.message.c_str()); return 1; }
    std::printf("%s: %zu bytes tick=%u\n",argv[i],game.bytes.size(),game.globals[4]);
    for(const auto& s:game.sections) std::printf("  %s: offset=%zu bytes=%zu records=%zu\n",s.name.c_str(),s.offset,s.size,s.records.size());
  }
  return 0;
}
