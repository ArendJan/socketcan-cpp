#include "socketcan_cpp/socketcan_cpp.h"
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

void check(scpp::SocketCanStatus sc_status)
{
    if (sc_status != scpp::STATUS_OK)
        printf("something went wrong, error code : %d \n", int32_t(sc_status));
    else
        printf("Message was written to the socket \n");
}

int main()
{
    scpp::BroadcastCan sockat_can;
    if (sockat_can.open("vcan0", 3, scpp::MODE_CANFD_MTU) == scpp::STATUS_OK)
    {
        scpp::CanFrame frame;
        frame.id = 0x15;
        frame.len = 50;
        frame.data[0] = 1;
        frame.data[1] = 2;
        frame.data[2] = 3;
        frame.data[49] = 4;
        frame.flags = 0;
        auto write_sc_status = sockat_can.setBroadcast(frame, 100); // set a first broadcast
        check(write_sc_status);
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));

        frame.id = 0x15;
        frame.data[0] = 255;
        write_sc_status = sockat_can.setBroadcast(frame, 100); // update a broadcast
        check(write_sc_status);
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
        frame.id = 0x16;
        write_sc_status = sockat_can.setBroadcast(frame, 100); // set a second broadcast
        check(write_sc_status);
        std::this_thread::sleep_for(std::chrono::milliseconds(1050));
        write_sc_status = sockat_can.removeBroadcast(0x16);
        check(write_sc_status);
        write_sc_status = sockat_can.removeBroadcast(0x20); // wont do anything, because this message is never set
        check(write_sc_status);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        sockat_can.close(); // close socket, so no more messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    else
    {
        perror("asdf");
        printf("Cannot open can socket!");
    }
    return 0;
}
