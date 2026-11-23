#pragma once
#include <string>
namespace notify {
class WsaGuard { bool owner_=false; public: WsaGuard(); ~WsaGuard(); WsaGuard(const WsaGuard&)=delete; WsaGuard& operator=(const WsaGuard&)=delete; };
struct ProxyResult { bool ok=false; double rtt_ms=0; std::string detail; std::string egress_ip; bool egress_verified=false; };
class ProxyPreflight { public: ProxyResult tcp_connect(const std::string&,unsigned,unsigned) const; ProxyResult http_connect(const std::string&,unsigned,const std::string&,unsigned,unsigned) const; ProxyResult socks5(const std::string&,unsigned,const std::string&,unsigned,unsigned) const; };
}
