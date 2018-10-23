// SPDX-License-Identifier: LGPL-3.0-only
#include "../rules.hh"

#include <cstring>
#include <unordered_map>
#include <queue>

#include <arpa/inet.h>

#ifdef SOCKET_ACTIVATION
#include <systemd/sd-daemon.h>

/*
 * Get a systemd socket file descriptor for the given rule either via name if
 * fd_name is set or just the next file descriptor available.
 */
int get_systemd_fd_for_rule(Rule rule)
{
    static std::unordered_map<std::string, int> names;
    static std::queue<int> fds;
    static bool fetch_done = false;

    if (!fetch_done) {
        char **raw_names = nullptr;
        int count = sd_listen_fds_with_names(1, &raw_names);
        if (count < 0) {
            fprintf(stderr, "FATAL: Unable to get systemd sockets: %s\n",
                    strerror(errno));
            std::abort();
        } else if (count == 0) {
            fputs("FATAL: Needed at least one systemd socket file descriptor,"
                  " but found zero.\n", stderr);
            std::abort();
        }
        for (int i = 0; i < count; ++i) {
            std::string name = raw_names[i];
            if (name.empty() || name == "unknown" || name == "stored")
                fds.push(SD_LISTEN_FDS_START + i);
            else
                names[name] = SD_LISTEN_FDS_START + i;
        }
        if (raw_names != nullptr)
            free(raw_names);
        fetch_done = true;
    }

    if (rule.fd_name) {
        auto found = names.find(rule.fd_name.value());
        if (found == names.end()) {
            fprintf(stderr, "FATAL: Can't get systemd socket for '%s'.\n",
                    rule.fd_name.value().c_str());
            std::abort();
        }
        return found->second;
    } else if (fds.empty()) {
        fputs("FATAL: Ran out of systemd sockets to assign\n", stderr);
        std::abort();
    }

    int fd = fds.front();
    fds.pop();
    return fd;
}
#endif