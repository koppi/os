/**
 * @file ssh.h
 * @brief A small in-kernel SSHv2 server, enough for a stock OpenSSH client to
 *        log in and reach the same command shell as the local console.
 *
 * Algorithms: curve25519-sha256 key exchange, ssh-ed25519 host key,
 * aes128-ctr cipher, hmac-sha2-256 MAC — all defaults a modern OpenSSH
 * client already offers, so no client-side flags are needed (beyond
 * accepting the host key on first connect). Authentication is a single
 * fixed username/password (see @c SSH_USERNAME / @c SSH_PASSWORD in ssh.c) —
 * there is no user database in this OS.
 *
 * A dedicated worker thread dequeues and serves accepted connections one at
 * a time, so a new connection is always accepted promptly even while a
 * prior session is still running -- but each session's actual traffic is
 * still marshalled onto the `net` thread (via @c net_exec, same as
 * ping/dns/http/nfs), since the IPv4/TCP stack has no locking and assumes
 * single-threaded access. An active session therefore still ties up the
 * `net` thread for its duration; only the accept/queue path is decoupled.
 */
#pragma once

/** @brief Called every iteration of the `net` thread: accepts new SSH
 *  connections and queues them for the worker thread. Never blocks. */
void ssh_tick(void);

/** @brief Worker thread entry point: dequeues pending connections and
 *  serves each one (via @c net_exec) in turn. */
void ssh_worker_func(void);

/** @brief One-line human readable status, for the `ssh` console command. */
const char *ssh_status_str(void);
