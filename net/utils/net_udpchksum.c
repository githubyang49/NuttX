/****************************************************************************
 * net/utils/net_udpchksum.c
 *
 *   Copyright (C) 2007-2010, 2012, 2014-2015, 2017 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/net/netconfig.h>
#include <nuttx/net/netdev.h>

#include "utils/utils.h"

#ifdef CONFIG_NET_UDP

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: udp_ipv4_chksum
 *
 * Description:
 *   Calculate the UDP/IPv4 checksum of the packet in d_buf and d_appdata.
 *
 ****************************************************************************/

#if defined(CONFIG_NET_UDP_CHECKSUMS) && defined(CONFIG_NET_IPv4)
uint16_t udp_ipv4_chksum(FAR struct net_driver_s *dev)
{
  return ipv4_upperlayer_chksum(dev, IP_PROTO_UDP);
}
#endif

/****************************************************************************
 * Name: udp_ipv6_chksum
 *
 * Description:
 *   Calculate the UDP/IPv6 checksum of the packet in d_buf and d_appdata.
 *
 ****************************************************************************/

#if defined(CONFIG_NET_UDP_CHECKSUMS) && defined(CONFIG_NET_IPv6)
uint16_t udp_ipv6_chksum(FAR struct net_driver_s *dev)
{
  return ipv6_upperlayer_chksum(dev, IP_PROTO_UDP, IPv6_HDRLEN);
}
#endif

#endif /* CONFIG_NET_UDP */
