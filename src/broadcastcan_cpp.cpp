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
#include <iostream>
#include <linux/can/raw.h>

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
        std::cout << "close bcm????????????????" << std::endl;
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

    SocketCanStatus BroadcastCan::setBroadcast(const CanFrame &msg, uint32_t intervalMs, int index)
    {

#ifdef HAVE_SOCKETCAN_HEADERS
        struct CanBCMFrameFD bcmFrame;

        // struct canfd_frame frame;
        memset(&bcmFrame, 0, sizeof(bcmFrame)); /* init CAN FD frame, e.g. LEN = 0 */

        // TODO: set bcmframe.header
        // TODO: find can id of header
        if (index == -1)
        {
            auto id = std::find(this->setIds.begin(), this->setIds.end(), msg.id);
            if (id != this->setIds.end())
            {
                bcmFrame.msg_head.flags = TX_ANNOUNCE; // if update, then announce instead of settimer|starttimer
                bcmFrame.msg_head.can_id = std::distance(this->setIds.begin(), id);
            }
            else
            {
                bcmFrame.msg_head.flags = SETTIMER | STARTTIMER; // new message, so settimer and starttimer
                this->setIds.push_back(msg.id);
                bcmFrame.msg_head.can_id = this->setIds.size() - 1;
            }
        } else {
            if (index < this->setIds.size())
            {
                bcmFrame.msg_head.flags = TX_ANNOUNCE; // if update, then announce instead of settimer|starttimer
                bcmFrame.msg_head.can_id = index;
            }
            else
            {
                bcmFrame.msg_head.flags = SETTIMER | STARTTIMER; // new message, so settimer and starttimer
                this->setIds.push_back(msg.id);
                bcmFrame.msg_head.can_id = index;
            }
        }
        bcmFrame.msg_head.ival1.tv_sec = 0;
        bcmFrame.msg_head.ival1.tv_usec = 0;
        bcmFrame.msg_head.ival2.tv_sec = intervalMs / 1000;
        bcmFrame.msg_head.ival2.tv_usec = (intervalMs % 1000) * 1000;
        bcmFrame.msg_head.count = 0;
        bcmFrame.msg_head.nframes = 1;
        bcmFrame.msg_head.opcode = TX_SETUP;
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
            bcmFrame.msg_head.flags |= CAN_FD_FRAME;
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
    SocketCanStatus BroadcastCan::removeBroadcast(const uint32_t id)
    {

#ifdef HAVE_SOCKETCAN_HEADERS
        struct bcm_msg_head bcmHeader;

        memset(&bcmHeader, 0, sizeof(bcmHeader));
        bcmHeader.opcode = TX_DELETE;

        auto index = std::find(this->setIds.begin(), this->setIds.end(), id);
        if (index != this->setIds.end())
        {
            bcmHeader.can_id = std::distance(this->setIds.begin(), index);
        }
        else
        {
            return STATUS_OK; // no message ever setup with this id.
        }
        if (this->m_socket_mode == MODE_CANFD_MTU)
        {
            bcmHeader.flags = CAN_FD_FRAME;
        }
        size_t writeSize = sizeof(bcmHeader); // only need to write the header
        if (::write(m_socket, &bcmHeader, writeSize) != writeSize)
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
