// SPDX-License-Identifier: GPL-3.0-or-later
#include "tcp_diagnostics.h"
#ifdef MT7697_NETWORK_TRACE
#include <atomic>
#endif
extern "C" {
#include <lwip/tcp_impl.h>
#include <lwip/tcpip.h>
#ifdef MT7697_NETWORK_TRACE
#include <lwip/inet_chksum.h>
void __real_tcp_input(struct pbuf*,struct netif*);
#endif
#include <lwip/sys.h>
#include <wifi_private_api.h>
// The prebuilt SDK uses 16-bit mem_size_t (36 KiB heap), unlike the
// distribution's TGN-enabled headers. Verified against linked SDK DWARF.
extern const uint16_t sdk_lwip_stats[] asm("lwip_stats");
}
namespace {
mt7697::TcpDiagnostics state;
#ifdef MT7697_NETWORK_TRACE
std::atomic<unsigned> ingress{0},input_errors{0};
uint32_t be32(const uint8_t* p) {return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
#endif
struct Request {mt7697::TcpDiagnostics* out;sys_sem_t done;};
void snapshot(void* arg) {
  auto& r=*static_cast<Request*>(arg);
  state.active=state.timewait=state.queued=state.retransmitted=0;
  state.stalled_port=state.stalled_ack=state.stalled_next=state.stalled_queue=0;
  for(auto* p=tcp_active_pcbs;p;p=p->next) {
    ++state.active;state.queued+=p->snd_queuelen;
    if(p->nrtx) {
      ++state.retransmitted;
      state.stalled_port=p->remote_port;state.stalled_ack=p->lastack;
      state.stalled_next=p->snd_nxt;state.stalled_queue=p->snd_queuelen;
    }
  }
  for(auto* p=tcp_tw_pcbs;p;p=p->next) ++state.timewait;
  state.pool_used=sdk_lwip_stats[86];state.pool_peak=sdk_lwip_stats[87];
  state.pool_errors=sdk_lwip_stats[84];
  *r.out=state;sys_sem_signal(&r.done);
}
}
namespace mt7697 {
#ifdef MT7697_NETWORK_TRACE
void record_tcp_ingress(const uint8_t* h,unsigned n,int error) {
  if(n<54 || h[12]!=8 || h[13] || h[14]>>4!=4 || h[23]!=6) return;
  unsigned i=14+(h[14]&15)*4;
  if(i<34 || n<i+20 || h[i+2]!=0 || h[i+3]!=80) return;
  ++ingress;if(error) ++input_errors;
}
#endif
bool tcp_diagnostics(TcpDiagnostics& out) {
  Request r{&out,{}};
  if(sys_sem_new(&r.done,0)!=ERR_OK) return false;
  if(tcpip_callback(snapshot,&r)!=ERR_OK) {sys_sem_free(&r.done);return false;}
  sys_arch_sem_wait(&r.done,0);sys_sem_free(&r.done);
#ifdef MT7697_NETWORK_TRACE
  out.ingress=ingress.load();out.input_errors=input_errors.load();
#endif
  uint8_t mode;
  if(wifi_config_get_power_save_mode(&mode)>=0) out.power_save=mode;
  return true;
}
}
#ifdef MT7697_NETWORK_TRACE
extern "C" void __wrap_tcp_input(struct pbuf* p,struct netif* iface) {
  uint8_t h[20]={};
  bool matched=false,valid=false;
  uint32_t next=0;
  bool watched=pbuf_copy_partial(p,h,sizeof(h),0)==sizeof(h) && h[2]==0 && h[3]==80;
  uint32_t remote=ip_current_src_addr()->addr;
  uint16_t port=uint16_t(h[0])<<8|h[1];
  if(watched) {
    ++state.processed;
    valid=!inet_chksum_pseudo(p,6,p->tot_len,ip_current_src_addr(),ip_current_dest_addr());
    if(!valid) ++state.bad_checksum;
    state.last_port=port;state.last_ack=be32(h+8);state.last_seq=be32(h+4);
    for(auto* pcb=tcp_active_pcbs;pcb;pcb=pcb->next) {
      if(pcb->local_port==80 && pcb->remote_port==port && pcb->remote_ip.addr==remote) {
        matched=true;next=pcb->snd_nxt;
        state.before_ack=pcb->lastack;state.last_rxnext=pcb->rcv_nxt;break;
      }
    }
  }
  __real_tcp_input(p,iface);
  if(watched) {
    for(auto* pcb=tcp_active_pcbs;pcb;pcb=pcb->next) {
      if(pcb->local_port==80 && pcb->remote_port==port && pcb->remote_ip.addr==remote) {
        state.after_ack=pcb->lastack;
        if(state.after_ack!=state.before_ack) ++state.advanced;
        if(matched && valid && (h[13]&16) && int32_t(state.last_ack-state.before_ack)>0 &&
           int32_t(next-state.last_ack)>=0 && state.after_ack==state.before_ack) {
          ++state.rejected;state.rejected_port=port;state.rejected_seq=state.last_seq;
          state.rejected_rxnext=state.last_rxnext;state.rejected_ack=state.last_ack;
          state.rejected_before=state.before_ack;state.rejected_next=next;
        }
        break;
      }
    }
  }
}
#endif
