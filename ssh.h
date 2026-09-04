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
 * Like @ref nfs_boot_tick, this runs entirely on the `net` kernel thread:
 * @ref ssh_tick polls for a new connection and, once one is accepted, runs
 * the whole session (auth, shell) to completion before returning. Only one
 * SSH session is handled at a time, and other `net`-thread work (DHCP
 * renewal, NFS, ntp) is paused for the duration of a session — consistent
 * with the rest of this single-threaded network stack.
 */
#pragma once

/** @brief Called every iteration of the `net` thread: accepts and services
 *  one SSH connection at a time on port 22. */
void ssh_tick(void);

/** @brief One-line human readable status, for the `ssh` console command. */
const char *ssh_status_str(void);
