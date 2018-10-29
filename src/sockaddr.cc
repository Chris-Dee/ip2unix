// SPDX-License-Identifier: LGPL-3.0-only
#include <cstring>
#include <stdexcept>
#include <optional>

#include <arpa/inet.h>

#include "sockaddr.hh"

SockAddr::SockAddr()
{
    memset(this, 0, sizeof(sockaddr_storage));
}

SockAddr::SockAddr(const sockaddr *addr)
    : SockAddr()
{
    memcpy(this, addr, sizeof(sockaddr_storage));
}

std::optional<SockAddr> SockAddr::create(const std::string &addr,
                                         uint16_t port,
                                         sa_family_t family)
{
    SockAddr sa;
    if (family != AF_INET && family != AF_INET6)
        return std::nullopt;
    sa.ss_family = family;
    if (!sa.set_host(addr))
        return std::nullopt;
    if (!sa.set_port(port))
        return std::nullopt;

    return sa;
}

SockAddr SockAddr::copy() const
{
    SockAddr sa(reinterpret_cast<const sockaddr*>(this));
    return sa;
}

std::optional<std::string> SockAddr::get_host(void) const
{
    if (this->ss_family == AF_INET) {
        const sockaddr_in *addr = this->ccast4();
        char buf[INET_ADDRSTRLEN];

        if (inet_ntop(addr->sin_family, &addr->sin_addr, buf,
                      INET_ADDRSTRLEN) != nullptr)
            return std::string(buf);
    } else if (this->ss_family == AF_INET6) {
        const sockaddr_in6 *addr = this->ccast6();
        char buf[INET6_ADDRSTRLEN];

        if (inet_ntop(addr->sin6_family, &addr->sin6_addr, buf,
                      INET6_ADDRSTRLEN) != nullptr)
            return std::string(buf);
    }

    return std::nullopt;
}

bool SockAddr::set_host(const std::string &host)
{
    if (this->ss_family == AF_INET) {
        sockaddr_in *addr = this->cast4();
        if (inet_pton(AF_INET, host.c_str(), &addr->sin_addr.s_addr) != 1)
            return false;
    } else if (this->ss_family == AF_INET6) {
        sockaddr_in6 *addr = this->cast6();
        if (inet_pton(AF_INET6, host.c_str(), &addr->sin6_addr.s6_addr) != 1)
            return false;
    } else {
        return false;
    }

    return true;
}

bool SockAddr::set_host(const SockAddr &other)
{
    if (this->ss_family == AF_INET && other.ss_family == AF_INET) {
        memcpy(&this->cast4()->sin_addr, &other.ccast4()->sin_addr,
               sizeof(in_addr));
        return true;
    } else if (this->ss_family == AF_INET6 && other.ss_family == AF_INET6) {
        memcpy(&this->cast6()->sin6_addr, &other.ccast6()->sin6_addr,
               sizeof(in6_addr));
        return true;
    } else {
        return false;
    }
}

bool SockAddr::set_host(const ucred &peercred)
{
    if (this->ss_family == AF_INET) {
        this->cast4()->sin_addr.s_addr = htonl(peercred.pid);
        return true;
    } else if (this->ss_family == AF_INET6) {
        sockaddr_in6 *addr = this->cast6();
        addr->sin6_addr.s6_addr[0] = 0xfe;
        addr->sin6_addr.s6_addr[1] = 0x80;
        addr->sin6_addr.s6_addr[2] = 0x00;
        addr->sin6_addr.s6_addr[3] = 0x00;
        uint32_t part = htonl(peercred.uid);
        memcpy(addr->sin6_addr.s6_addr + 4, &part, 4);
        part = htonl(peercred.gid);
        memcpy(addr->sin6_addr.s6_addr + 8, &part, 4);
        part = htonl(peercred.pid);
        memcpy(addr->sin6_addr.s6_addr + 12, &part, 4);
        return true;
    }
    return false;
}

std::optional<uint16_t> SockAddr::get_port(void) const
{
    if (this->ss_family == AF_INET)
        return htons(this->ccast4()->sin_port);
    else if (this->ss_family == AF_INET6)
        return htons(this->ccast6()->sin6_port);
    else
        return std::nullopt;
}

bool SockAddr::set_port(uint16_t port)
{
    if (this->ss_family == AF_INET)
        this->cast4()->sin_port = htons(port);
    else if(this->ss_family == AF_INET6)
        this->cast6()->sin6_port = htons(port);
    else
        return false;

    return true;
}

bool SockAddr::is_loopback(void) const
{
    if (this->ss_family == AF_INET) {
        return (ntohl(this->ccast4()->sin_addr.s_addr) & 0xff000000)
               >> 24 == 127;
    } else if (this->ss_family == AF_INET6) {
        return IN6_IS_ADDR_LOOPBACK(&this->ccast6()->sin6_addr);
    } else {
        return false;
    }
}

void SockAddr::apply_addr(struct sockaddr *addr, socklen_t *addrlen) const
{
    if (this->ss_family == AF_INET)
        *addrlen = sizeof(sockaddr_in);
    else if (this->ss_family == AF_INET6)
        *addrlen = sizeof(sockaddr_in6);
    else
        return;

    memcpy(addr, this, *addrlen);
}
