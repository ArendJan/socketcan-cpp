#include "socketcan_cpp/socketcan_cpp.h"
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
int main()
{
    scpp::BroadcastCan sockat_can;
    if (sockat_can.open("vcan0", 3, scpp::MODE_CAN_MTU) == scpp::STATUS_OK)
    {
        scpp::CanFrame frame;
        frame.id = 0x15;
        frame.len = 4;
        frame.data[0] = 1;
        frame.data[1] = 2;
        frame.data[2] = 3;
        frame.data[3] = 4;
        frame.flags = 0;
        auto write_sc_status = sockat_can.setBroadcast(frame, 100);

        if (write_sc_status != scpp::STATUS_OK)
            printf("something went wrong on socket write, error code : %d \n", int32_t(write_sc_status));
        else
            printf("Message was written to the socket \n");

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        frame.id = 0x16;
        write_sc_status = sockat_can.setBroadcast(frame, 100);
        if (write_sc_status != scpp::STATUS_OK)
            printf("something went wrong on socket write, error code : %d \n", int32_t(write_sc_status));
        else
            printf("Message was written to the socket \n");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        frame.id = 0x15;
        frame.data[0] = 255;
        write_sc_status = sockat_can.setBroadcast(frame, 100);
        if (write_sc_status != scpp::STATUS_OK)
            printf("something went wrong on socket write, error code : %d \n", int32_t(write_sc_status));
        else
            printf("Message was written to the socket \n");
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    else
    {
        printf("Cannot open can socket!");
    }
    return 0;
}
