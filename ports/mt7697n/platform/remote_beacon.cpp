// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_protocol.h"
#include <string.h>
extern "C" {
#include <t_bearssl.h>
}
namespace mt7697 { namespace remote {
size_t decrypt(const Beacon& b, const uint8_t key[12], uint8_t out[24]) {
  // This key format is for stock legacy frames (versions 2/3). Newer frames
  // require a different 16-byte key; never accept their ciphertext as plaintext.
  if (!(b.flags&8) || !(b.flags&0x40) || (b.flags>>12)>3 ||
      b.size<b.payload_offset+7 || b.size>b.payload_offset+28) return 0;
  const size_t n=b.size-b.payload_offset-4;
  if (n>24) return 0;
  uint8_t aes[16], nonce[13], tag[16];
  memcpy(aes,key,6);
  const uint8_t middle[]={0x8d,0x3d,0x3c,0x97};
  memcpy(aes+6,middle,4); memcpy(aes+10,key+6,6);
  memcpy(nonce,b.frame,5); memcpy(nonce+5,b.frame+b.size-4,3);
  memcpy(nonce+8,b.mac,5);
  br_aes_small_ctrcbc_keys cipher;
  br_aes_small_ctrcbc_init(&cipher,aes,sizeof(aes));
  br_ccm_context ccm;
  br_ccm_init(&ccm,&cipher.vtable);
  // Stock 0x100A996A encodes ((tag_size-2)>>1)&7 in B0. With tag_size=1
  // this is 7, the same B0 as a 16-byte CCM tag, then compares only byte 0.
  if (!br_ccm_reset(&ccm,nonce,sizeof(nonce),1,n,16)) return 0;
  const uint8_t aad=0x11;
  br_ccm_aad_inject(&ccm,&aad,1); br_ccm_flip(&ccm);
  memcpy(out,b.frame+b.payload_offset,n);
  br_ccm_run(&ccm,0,out,n); br_ccm_get_tag(&ccm,tag);
  if (tag[0]!=b.frame[b.size-1]) { memset(out,0,n); return 0; }
  for (size_t pos=0; pos<n;) {
    if (n-pos<3 || out[pos+2]>n-pos-3) { memset(out,0,n); return 0; }
    pos+=3+out[pos+2];
  }
  return n;
}
} }
