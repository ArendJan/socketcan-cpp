#include "../include/socketcan_cpp/socketcan_cpp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#ifdef HAVE_SOCKETCAN_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can/raw.h>
/* CAN DLC to real data length conversion helpers */

static const unsigned char dlc2len[] = {0, 1, 2, 3, 4, 5, 6, 7,
                                        8, 12, 16, 20, 24, 32, 48, 64};

/* get data length from can_dlc with sanitized can_dlc */
unsigned char can_dlc2len(unsigned char can_dlc)
{
    return dlc2len[can_dlc & 0x0F];
}

static const unsigned char len2dlc[] = {0, 1, 2, 3, 4, 5, 6, 7, 8,       /* 0 - 8 */
                                        9, 9, 9, 9,                      /* 9 - 12 */
                                        10, 10, 10, 10,                  /* 13 - 16 */
                                        11, 11, 11, 11,                  /* 17 - 20 */
                                        12, 12, 12, 12,                  /* 21 - 24 */
                                        13, 13, 13, 13, 13, 13, 13, 13,  /* 25 - 32 */
                                        14, 14, 14, 14, 14, 14, 14, 14,  /* 33 - 40 */
                                        14, 14, 14, 14, 14, 14, 14, 14,  /* 41 - 48 */
                                        15, 15, 15, 15, 15, 15, 15, 15,  /* 49 - 56 */
                                        15, 15, 15, 15, 15, 15, 15, 15}; /* 57 - 64 */

/* map the sanitized data length to an appropriate data length code */
unsigned char can_len2dlc(unsigned char len)
{
    if (len > 64)
        return 0xF;

    return len2dlc[len];
}

#endif

namespace scpp
{
    BroadcastCan::BroadcastCan()
    {
    }
    SocketCanStatus BroadcastCan::open(const std::string &can_interface, int32_t read_timeout_ms, SocketMode mode)
    {
        m_interface = can_interface;
        m_socket_mode = mode;
        m_read_timeout_ms = read_timeout_ms;
#ifdef HAVE_SOCKETCAN_HEADERS

        /* open socket */
        if ((m_socket = socket(PF_CAN, SOCK_DGRAM, CAN_BCM)) < 0)
        {
            perror("socket");
            return STATUS_SOCKET_CREATE_ERROR;
        }
        int mtu, enable_canfd = 1;
        struct sockaddr_can addr;
        struct ifreq ifr;

        strncpy(ifr.ifr_name, can_interface.c_str(), IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
        ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
        if (!ifr.ifr_ifindex)
        {
            perror("if_nametoindex");
            return STATUS_INTERFACE_NAME_TO_IDX_ERROR;
        }

        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (mode == MODE_CANFD_MTU)
        {
            /* check if the frame fits into the CAN netdevice */
            if (ioctl(m_socket, SIOCGIFMTU, &ifr) < 0)
            {
                perror("SIOCGIFMTU");
                return STATUS_MTU_ERROR;
            }
            mtu = ifr.ifr_mtu;

            if (mtu != CANFD_MTU)
            {
                return STATUS_CANFD_NOT_SUPPORTED;
            }

            /* interface is ok - try to switch the socket into CAN FD mode */
            if (setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                           &enable_canfd, sizeof(enable_canfd)))
            {

                return STATUS_ENABLE_FD_SUPPORT_ERROR;
            }
        }

        struct timeval tv;
        tv.tv_sec = 0;                         /* 30 Secs Timeout */
        tv.tv_usec = m_read_timeout_ms * 1000; // Not init'ing this can cause strange errors
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(struct timeval));

        if (connect(m_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            return STATUS_BIND_ERROR;
        }

#else
        printf("Your operating system does not support socket can! \n");
#endif
        return STATUS_OK;
    }

    SocketCanStatus BroadcastCan::close()
    {
#ifdef HAVE_SOCKETCAN_HEADERS
        ::close(m_socket);
#endif
        return STATUS_OK;
    }
    const std::string &BroadcastCan::interfaceName() const
    {
        return m_interface;
    }
    BroadcastCan::~BroadcastCan()
    {
        close();
    }

    SocketCanStatus BroadcastCan::setBroadcast(const CanFrame &msg, uint32_t intervalMs)
    {

#ifdef HAVE_SOCKETCAN_HEADERS
        struct CanBCMFrameFD bcmFrame;

        // struct canfd_frame frame;
        memset(&bcmFrame, 0, sizeof(bcmFrame)); /* init CAN FD frame, e.g. LEN = 0 */

        // TODO: set bcmframe.header
        // TODO: find can id of header
        auto id = std::find(this->setIds.begin(), this->setIds.end(), msg.id);
        if (id != this->setIds.end())
        {
            bcmFrame.msg_head.can_id = std::distance(this->setIds.begin(), id);
        }
        else
        {
            this->setIds.push_back(msg.id);
            bcmFrame.msg_head.can_id = this->setIds.size() - 1;
        }
        bcmFrame.msg_head.ival1.tv_sec = 0;
        bcmFrame.msg_head.ival1.tv_usec = 0;
        bcmFrame.msg_head.ival2.tv_sec = intervalMs / 1000;
        bcmFrame.msg_head.ival2.tv_usec = (intervalMs % 1000) * 1000;
        bcmFrame.msg_head.count = 0;
        bcmFrame.msg_head.nframes = 1;
        bcmFrame.msg_head.opcode = TX_SETUP;
        bcmFrame.msg_head.flags = SETTIMER | STARTTIMER;
        bcmFrame.frame.can_id = msg.id;
        bcmFrame.frame.len = msg.len;
        bcmFrame.frame.flags = msg.flags;
        memcpy(bcmFrame.frame.data, msg.data, msg.len);
        size_t writeSize = sizeof(CanBCMFrame);
        if (m_socket_mode == MODE_CANFD_MTU)
        {
            /* ensure discrete CAN FD length values 0..8, 12, 16, 20, 24, 32, 64 */
            bcmFrame.frame.len = can_dlc2len(can_len2dlc(bcmFrame.frame.len));
            writeSize = sizeof(CanBCMFrameFD);
        }

        if (::write(m_socket, &bcmFrame, writeSize) != writeSize)
        {
            perror("write");
            return STATUS_WRITE_ERROR;
        }

#else
        printf("Your operating system does not support socket can! \n");
#endif
        return STATUS_OK;
    }

}
