// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_protocol.h"
#include <string.h>

namespace mt7697 { namespace remote {
uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
void rc4(const uint8_t* key, size_t key_size, const uint8_t* in, uint8_t* out, size_t size) {
  uint8_t s[256], j = 0;
  for (unsigned i=0; i<256; ++i) s[i]=i;
  for (unsigned i=0; i<256; ++i) {
    j=uint8_t(j+s[i]+key[i%key_size]);
    const uint8_t t=s[i]; s[i]=s[j]; s[j]=t;
  }
  uint8_t i=0; j=0;
  while (size--) {
    ++i; j=uint8_t(j+s[i]);
    const uint8_t t=s[i]; s[i]=s[j]; s[j]=t;
    *out++=*in++ ^ s[uint8_t(s[i]+s[j])];
  }
}
void wrap_token(bool second, const uint8_t m[6], uint16_t pid,
                const uint8_t in[12], uint8_t out[12]) {
  const uint8_t lo=pid, hi=pid>>8;
  const uint8_t a[]={m[0],m[2],m[5],lo,lo,m[4],m[5],m[1]};
  const uint8_t b[]={m[0],m[2],m[5],hi,m[4],m[0],m[5],lo};
  rc4(second?b:a,8,in,out,12);
}
bool parse(const uint8_t* ad, size_t size, const uint8_t address[6], Beacon& out) {
  if (size>31) return false;
  for (size_t pos=0; pos<size;) {
    const size_t n=ad[pos++];
    if (!n) break;
    if (n>size-pos) return false;
    if (n>=8 && ad[pos]==0x16 && le16(ad+pos+1)==0xfe95) {
      Beacon b;
      b.frame=ad+pos+3; b.size=n-3;
      b.flags=le16(b.frame); b.pid=le16(b.frame+2); b.sequence=b.frame[4];
      if (b.pid!=0x0153 && b.pid!=0x03b6) return false;
      const unsigned version=b.flags>>12;
      if (version<2 || version>5) return false;
      b.payload_offset=5;
      memcpy(b.mac,address,6);
      if (b.flags&0x10) {
        if (b.size<11) return false;
        memcpy(b.mac,b.frame+5,6); b.payload_offset=11;
        // The link-layer peer address and authenticated device identity must agree.
        if (memcmp(b.mac,address,6)) return false;
      }
      if (b.flags&0x20) {
        if (b.payload_offset>=b.size) return false;
        ++b.payload_offset; // Stock pairing parser skips one capability byte.
      }
      if (b.payload_offset>b.size) return false;
      out=b; return true;
    }
    pos+=n;
  }
  return false;
}
bool pairing_request(const Beacon& b) {
  if ((b.flags&0x08) || !(b.flags&0x40)) return false;
  for (size_t p=b.payload_offset; p+3<=b.size;) {
    const uint16_t id=le16(b.frame+p);
    const size_t n=b.frame[p+2]; p+=3;
    if (n>b.size-p) return false;
    if (id==2) return n==2 && le16(b.frame+p)==0x1001;
    p+=n;
  }
  return false;
}
Request Authentication::write(uint16_t uuid, const uint8_t* data, size_t size) {
  Request r; r.op=Op::Write; r.uuid=uuid; r.size=size;
  memcpy(r.data,data,size); return r;
}
Request Authentication::abort() {
  state_=State::Failed; early_size_=0;
  memset(token_,0,sizeof(token_)); memset(session_,0,sizeof(session_));
  memset(key_,0,sizeof(key_));
  Request r; r.op=Op::Failed; return r;
}
Request Authentication::begin(const uint8_t mac[6], uint16_t pid,
                              const uint8_t token[12], bool login) {
  *this=Authentication{};
  memcpy(mac_,mac,6); memcpy(token_,token,12); pid_=pid; login_=login;
  if (login) {
    state_=State::Subscribe;
    Request r; r.op=Op::Subscribe; r.uuid=1; return r;
  }
  state_=State::Hello;
  const uint8_t hello[]={0x90,0xca,0x85,0xde};
  return write(0x10,hello,4);
}
Request Authentication::complete(bool success, const uint8_t* data, size_t size) {
  if (!success) return abort();
  switch (state_) {
    case State::Hello: {
      state_=State::Subscribe;
      Request r; r.op=Op::Subscribe; r.uuid=1; return r;
    }
    case State::Subscribe: {
      if (login_) {
        state_=State::LoginHello;
        const uint8_t hello[]={0x00,0xbc,0x43,0xcd};
        return write(0x10,hello,4);
      }
      state_=State::Token;
      uint8_t wrapped[12]; wrap_token(false,mac_,pid_,token_,wrapped);
      return write(1,wrapped,12);
    }
    case State::Token: state_=State::Proof; break;
    case State::LoginHello: state_=State::Challenge; break;
    case State::LoginReply: state_=State::Ack; break;
    case State::Finish: {
      state_=State::Key;
      Request r; r.op=Op::Read; r.uuid=0x14; return r;
    }
    case State::Key: {
      if (size!=12) return abort();
      rc4(token_,12,data,key_,12); state_=State::Done;
      Request r; r.op=Op::Done; return r;
    }
    default: return abort();
  }
  if (early_size_) {
    const size_t n=early_size_; early_size_=0;
    return consume_notification(early_,n);
  }
  return {};
}
Request Authentication::notification(const uint8_t* data, size_t size) {
  if (state_==State::Token || state_==State::LoginHello || state_==State::LoginReply) {
    const size_t expected=state_==State::Token?12:4;
    if (size!=expected || early_size_) return abort();
    memcpy(early_,data,size); early_size_=size;
    return {};
  }
  return consume_notification(data,size);
}
Request Authentication::consume_notification(const uint8_t* data, size_t size) {
  uint8_t decoded[12]={};
  if (state_==State::Proof && size==12) {
    wrap_token(true,mac_,pid_,data,decoded);
    wrap_token(false,mac_,pid_,decoded,decoded);
    uint8_t difference=0;
    for (unsigned i=0; i<12; ++i) difference|=decoded[i]^token_[i];
    if (difference) return abort();
    const uint8_t finish[]={0x92,0xab,0x54,0xfa};
    rc4(token_,12,finish,decoded,4); state_=State::Finish;
    return write(1,decoded,4);
  }
  if (state_==State::Challenge && size==4) {
    rc4(token_,12,data,decoded,4); memcpy(session_,token_,12);
    for (unsigned i=0; i<4; ++i) session_[i]^=decoded[i];
    const uint8_t reply[]={0x09,0xac,0xbf,0x93};
    rc4(session_,12,reply,decoded,4); state_=State::LoginReply;
    return write(1,decoded,4);
  }
  if (state_==State::Ack && size==4) {
    rc4(session_,12,data,decoded,4);
    const uint8_t ack[]={0xc9,0x58,0x9a,0x36};
    if (memcmp(decoded,ack,4)) return abort();
    state_=State::Key;
    Request r; r.op=Op::Read; r.uuid=0x14; return r;
  }
  return abort();
}
} }
