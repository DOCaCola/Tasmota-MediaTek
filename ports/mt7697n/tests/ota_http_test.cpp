// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota_http_fakes.h"
int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  const std::vector<uint8_t> package((std::istreambuf_iterator<char>(file)), {});
  assert(package.size() > kOtaHeader);
  OtaUrl url;
  assert(parse_ota_url("http://192.0.2.2:8000/test.bin?x=1", url));
  assert(url.port == 8000 && url.ip[3] == 2 && !strcmp(url.path, "/test.bin?x=1"));
  for (auto bad : {"https://10.0.0.1/a", "http://host/a", "http://256.0.0.1/a",
                   "http://10.0.0.1:0/a", "http://10.0.0.1:65536/a",
                   "http://10.0.0.1/a\r\nX: y", "http://10.0.0.1/a#x", "http://10.0.0.1",
                   "http://10.0.0", "http://224.0.0.1/a", "http://10.0.0.1:999999999/a"}) {
    assert(!parse_ota_url(bad,url));
  }
  // Exercise every parser boundary with 1-byte to multi-sector network chunks.
  for (size_t fragment : {1, 2, 155, 156, 157, 701, 1024}) {
    MemoryFlash f; OtaStager s(f); Transport t; OtaDownload d(t,s);
    t.fragment = fragment; t.response = response(package);
    assert(d.start("http://192.0.2.2:8000/test.bin", 0));
    assert(!d.start("http://192.0.2.2/test.bin", 0));
    run(d);
    assert(d.state() == OtaDownloadState::Ready && s.state() == OtaState::Ready);
    assert(t.closed && !f.activations);
    assert(t.request.find("GET /test.bin HTTP/1.1\r\nHost: 192.0.2.2:8000\r\n") == 0);
    assert(!d.start("http://192.0.2.2/test.bin", 0));
  }
  for (auto headers : {"Transfer-Encoding: chunked\r\n", "Content-Encoding: gzip\r\n",
                       "Content-Length: 1\r\n", "Bad header\r\n"}) {
    MemoryFlash f; OtaStager s(f); Transport t; OtaDownload d(t,s);
    t.response = response(package,headers);
    assert(d.start("http://192.0.2.2/a",0)); run(d);
    assert(d.state() == OtaDownloadState::Failed && !f.writes && !f.activations);
  }
  for (auto headers : {"HTTP/1.1 302 Found\r\nContent-Length: 8192\r\n\r\n",
                       "HTTP/1.1 200 OK\r\n\r\n",
                       "HTTP/1.1 200 OK\r\nContent-Length: 42949672960\r\n\r\n",
                       "HTTP/1.1 200 OK\nContent-Length: 8192\n\n"}) {
    MemoryFlash f; OtaStager s(f); Transport t; OtaDownload d(t,s);
    t.response.assign(headers,headers+strlen(headers));
    assert(d.start("http://192.0.2.2/a",0)); run(d);
    assert(d.state() == OtaDownloadState::Failed && !f.writes);
  }
  // Truncation after writes, retry from scratch, and checksum failure.
  MemoryFlash f; OtaStager s(f); Transport t; OtaDownload d(t,s);
  t.response = response(package); t.response.resize(t.response.size()-1);
  assert(d.start("http://192.0.2.2/a",0)); run(d);
  assert(d.state() == OtaDownloadState::Failed && f.writes && !f.activations);
  t.response = response(package); t.response.back() ^= 1; t.pos = 0;
  assert(d.start("http://192.0.2.2/a",0)); run(d);
  assert(d.staging_result() == OtaResult::ChecksumMismatch && !f.activations);
  t.response = response(package); t.pos = 0;
  assert(d.start("http://192.0.2.2/a",0)); run(d);
  assert(d.state() == OtaDownloadState::Ready);
  for (unsigned mode = 0; mode < 4; ++mode) {
    MemoryFlash ff; OtaStager ss(ff); Transport tt; OtaDownload dd(tt,ss);
    tt.response = response(package); tt.stall = true;
    if (mode == 2) tt.connection = 0;
    if (mode == 3) tt.connection = -1;
    assert(dd.start("http://192.0.2.2/a",UINT32_MAX-100));
    dd.poll(UINT32_MAX-100,true);
    dd.poll(15000-101,mode != 1);
    assert(dd.state() == OtaDownloadState::Failed && tt.closed && !ff.activations);
  }
  for (const auto& h : {std::string(300,'x') + "\r\n",
                        std::string("X-Long: ") + std::string(250,'x') + "\r\n",
                        std::string(6000,'x')}) {
    MemoryFlash ff; OtaStager ss(ff); Transport tt; OtaDownload dd(tt,ss);
    tt.response=response(package,h);
    assert(dd.start("http://192.0.2.2/a",0)); run(dd);
    assert(dd.state()==OtaDownloadState::Failed && !ff.activations);
  }
  // Progress does not extend the five-minute absolute deadline.
  {
    MemoryFlash ff; OtaStager ss(ff); Transport tt; OtaDownload dd(tt,ss);
    tt.response=response(package); tt.fragment=1;
    assert(dd.start("http://192.0.2.2/a",0));
    for (uint32_t time=0; time<=300000 && dd.busy(); time+=10000) dd.poll(time,true);
    assert(dd.state()==OtaDownloadState::Failed && !ff.activations);
  }
  // A server cannot smuggle trailing bytes within the same network read.
  {
    MemoryFlash ff; OtaStager ss(ff); Transport tt; OtaDownload dd(tt,ss);
    tt.response=response(package); tt.response.push_back(0); tt.fragment=1024;
    assert(dd.start("http://192.0.2.2/a",0)); run(dd);
    assert(dd.state()==OtaDownloadState::Failed && !ff.activations);
  }
  puts("OTA HTTP tests passed: fragmentation, framing, disconnect, retry, checksum, timeouts");
}
