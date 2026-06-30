#include <zephyr/kernel.h>
#include "control.hpp"
#include "layers/transport/transportLayer.hpp"
#include "layers/transport/eventDispatcher.hpp"
#include "link_defines.h"
#include <zephyr/kernel.h>

#define STACK_SIZE 1024
#define PRIORITY   5

K_THREAD_STACK_DEFINE(my_stack, STACK_SIZE);
struct k_thread my_thread_data;

TransportControl g_transport = TransportControl();
EventDispatcher g_eventDispatcher = EventDispatcher(g_transport);
Control g_control = Control(g_eventDispatcher);

void receiveThread(void *arg1, void *arg2, void *arg3)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;

    while (true) 
    {
        auto call = g_transport.receiveCall();
        g_control.acceptCall(call);
    }
}



int main(void)
{
    k_thread_create(
        &my_thread_data,
        my_stack,
        K_THREAD_STACK_SIZEOF(my_stack),
        receiveThread,
        NULL, NULL, NULL,
        PRIORITY,
        0,              // no special options
        K_NO_WAIT       // start immediately
    );

    while (true)
    {
        g_control.executeMode();
    }

    return 0;
}