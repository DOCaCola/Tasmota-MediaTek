// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/remote_protocol.h"
#include "remote_beacon_vectors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
using namespace mt7697::remote;

static std::vector<uint8_t> hex(const char* s) {
  std::vector<uint8_t> out;
  while (*s) { unsigned v; assert(sscanf(s,"%2x",&v)==1); out.push_back(v); s+=2; }
  return out;
}
static void payload(const Request& r,Op op,uint16_t uuid,const char* bytes) {
  auto expected=hex(bytes);
  assert(r.op==op && r.uuid==uuid && r.size==expected.size());
  assert(!memcmp(r.data,expected.data(),expected.size()));
}
int main() {
  const auto mac=hex("102132435465"), token=hex("78563412efcdab9040302010");
  const auto key=hex("00112233445566778899aabb");
  for (bool early:{false,true}) {
    Authentication a;
    payload(a.begin(mac.data(),0x153,token.data(),false),Op::Write,0x10,"90ca85de");
    assert(a.complete(true).op==Op::Subscribe);
    auto r=a.complete(true);
    // Literal ciphertext from the original ARM oracle, not our wrapper.
    payload(r,Op::Write,1,"7449bd438a89519b40cbbe1d");
    uint8_t proof[12];
    wrap_token(true,mac.data(),0x153,r.data,proof);
    if (early) {
      assert(a.notification(proof,12).op==Op::None);
      r=a.complete(true);
    } else {
      assert(a.complete(true).op==Op::None);
      r=a.notification(proof,12);
    }
    payload(r,Op::Write,1,"3ea1b017");
    r=a.complete(true); assert(r.op==Op::Read && r.uuid==0x14);
    uint8_t encrypted[12]; rc4(token.data(),12,key.data(),encrypted,12);
    assert(a.complete(true,encrypted,12).op==Op::Done);
    assert(!memcmp(a.key(),key.data(),12));
  }
  const auto other=hex("a1b2c3d4e5f6");
  Authentication second;
  second.begin(other.data(),0x3b6,token.data(),false); second.complete(true);
  payload(second.complete(true),Op::Write,1,"21f60dfc2af3e7777f866967");
  for (unsigned n=0;n<25;++n) {
    Authentication a; uint8_t input[25]={};
    a.begin(mac.data(),0x153,token.data(),false); a.complete(true); a.complete(true);
    a.complete(true);
    assert(a.notification(input,n).op==Op::Failed);
    for (unsigned i=0;i<12;++i) assert(!a.token()[i] && !a.key()[i]);
  }
  for (bool early:{false,true}) {
    Authentication a;
    assert(a.begin(mac.data(),0x153,token.data(),true).op==Op::Subscribe);
    payload(a.complete(true),Op::Write,0x10,"00bc43cd");
    const uint8_t nonce[]={0x12,0x34,0x56,0x78};
    uint8_t encrypted[4], session[12]; memcpy(session,token.data(),12);
    for (unsigned i=0;i<4;++i) session[i]^=nonce[i];
    rc4(token.data(),12,nonce,encrypted,4);
    if (early) assert(a.notification(encrypted,4).op==Op::None);
    auto r=a.complete(true);
    if (!early) r=a.notification(encrypted,4);
    uint8_t expected[4]; const uint8_t reply[]={9,0xac,0xbf,0x93};
    rc4(session,12,reply,expected,4);
    assert(r.op==Op::Write && !memcmp(r.data,expected,4));
    const uint8_t ack[]={0xc9,0x58,0x9a,0x36};
    rc4(session,12,ack,encrypted,4);
    if (early) assert(a.notification(encrypted,4).op==Op::None);
    r=a.complete(true);
    if (!early) r=a.notification(encrypted,4);
    assert(r.op==Op::Read);
    assert(a.complete(false).op==Op::Failed);
  }
  for (const auto& v:beacon_vectors) {
    assert(v.size>0 && v.size<=31);
    Beacon b; uint8_t output[24]={};
    assert(parse(v.ad,v.size,v.mac,b));
    assert(decrypt(b,v.key,output)==6 && !memcmp(output,v.plain,6));
    uint8_t bad[31]; memcpy(bad,v.ad,v.size); bad[v.size-1]^=1;
    assert(parse(bad,v.size,v.mac,b));
    assert(!decrypt(b,v.key,output)); // No "plausible plaintext" fallback.
    for (unsigned n=0;n<v.size;++n) {
      if (parse(v.ad,n,v.mac,b)) assert(!decrypt(b,v.key,output));
    }
    memcpy(bad,v.ad,v.size); bad[4]|=0x80;
    assert(parse(bad,v.size,v.mac,b));
    assert(!decrypt(b,v.key,output)); // Header is authenticated too.
  }
  // Pairing advertisement, optional MAC, valid AD boundary and object length.
  auto ad=hex("0d1695fe40305301010200020110");
  Beacon b; assert(parse(ad.data(),ad.size(),mac.data(),b) && pairing_request(b));
  ad.back()=0; assert(parse(ad.data(),ad.size(),mac.data(),b) && !pairing_request(b));
  ad[11]=30; assert(parse(ad.data(),ad.size(),mac.data(),b) && !pairing_request(b));
  ad=hex("0e1695fe6030530101200200020110");
  assert(parse(ad.data(),ad.size(),mac.data(),b) && pairing_request(b));
  puts("Remote protocol: stock ciphertext, early notifications, failures, 20 ARM beacon vectors and malformed frames pass");
}
