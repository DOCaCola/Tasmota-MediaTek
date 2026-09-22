// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
namespace mt7697 {
struct TcpDiagnostics {
  uint32_t ingress=0,input_errors=0,processed=0,bad_checksum=0,advanced=0;
  uint32_t active=0,timewait=0,queued=0,retransmitted=0;
  uint32_t last_ack=0,before_ack=0,after_ack=0,last_seq=0,last_rxnext=0;
  uint32_t stalled_port=0,stalled_ack=0,stalled_next=0,stalled_queue=0;
  uint16_t last_port=0;
  uint32_t rejected=0,rejected_port=0,rejected_seq=0,rejected_rxnext=0;
  uint32_t rejected_ack=0,rejected_before=0,rejected_next=0;
  int power_save=-1;
};
bool tcp_diagnostics(TcpDiagnostics& output);
void record_tcp_ingress(const uint8_t* header,unsigned length,int error);
}
