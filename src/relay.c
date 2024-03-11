#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "multicast.h"
#include <stdio.h>
#include <unistd.h>

// #define APP_SEND_CHANNEL_IP "224.1.1.1"
#define SERVICE_LPORT 1100
// #define SERVICE_SEND_CHANNEL_IP "225.1.1.1"
#define APP_LPORT 1700
#define UNUSED_PORT 1000

// Define a struct for stack elements
typedef struct StackElement {
    char *message;
    struct StackElement *next;
} StackElement;

// Define a struct for the stack
typedef struct {
    StackElement *top;
    pthread_mutex_t lock;
} MessageStack;

// Initialize the stack
void initStack(MessageStack *stack) {
    stack->top = NULL;
    pthread_mutex_init(&stack->lock, NULL);
}

// Push a message onto the stack
void pushStack(MessageStack *stack, const char *message) {
    pthread_mutex_lock(&stack->lock);
    StackElement *newElement = (StackElement *)malloc(sizeof(StackElement));
    newElement->message = strdup(message); // Copy the message
    newElement->next = stack->top;
    stack->top = newElement;
    pthread_mutex_unlock(&stack->lock);
}

// Pop a message from the stack
char *popStack(MessageStack *stack) {
    pthread_mutex_lock(&stack->lock);
    if (stack->top == NULL) {
        pthread_mutex_unlock(&stack->lock);
        return NULL;
    }
    StackElement *topElement = stack->top;
    char *message = topElement->message;
    stack->top = topElement->next;
    free(topElement);
    pthread_mutex_unlock(&stack->lock);
    return message;
}
// Global message stack
MessageStack messageStack;

// Function for the listener thread
void *listenerThread(void *arg) {
    mcast_t *recvChannel = (mcast_t *)arg;
    char buffer[1024];
    
    while (true) {
        int msg_length = multicast_receive(recvChannel, buffer, sizeof(buffer));
        if (msg_length > 0) {
            printf("RECEIVED SOMETHING\n");
            buffer[msg_length] = '\0'; // Ensure null-termination
            pushStack(&messageStack, buffer);
        }
    }
    return NULL;
}

// Function for the forwarder thread
void *forwarderThread(void *arg) {
    mcast_t *sendChannel = (mcast_t *)arg;
    
    while (true) {
        char *message = popStack(&messageStack);
        if (message) {
            multicast_send(sendChannel, message, strlen(message));
            free(message); // Free the message after sending
        } else {
            // No messages in the stack, sleep to avoid busy waiting
            usleep(100000); // Sleep for 0.1 seconds
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <Recv_IP_LAN1> <Send_IP_LAN1> <Recv_IP_LAN2> <Send_IP_LAN2>\n", argv[0]);
        return 1;
    }

    // Initialize the global stack
    initStack(&messageStack);

    // Setup multicast channels for LAN 1
    // Listen on LAN 1, Receive Port
    mcast_t *recvChannel1 = multicast_init(argv[1], SERVICE_LPORT, UNUSED_PORT);
    multicast_setup_recv(recvChannel1);
    // Send to LAN 2, Send Port
    mcast_t *sendChannel1 = multicast_init(argv[2], UNUSED_PORT, APP_LPORT);

    // Setup multicast channels for LAN 2
    // Listen on LAN 2, Receive Port
    mcast_t *recvChannel2 = multicast_init(argv[3], SERVICE_LPORT, UNUSED_PORT);
    multicast_setup_recv(recvChannel2);
    // Send to LAN 1, Send Port
    mcast_t *sendChannel2 = multicast_init(argv[4], UNUSED_PORT, APP_LPORT);

    // Start listener and forwarder threads for each channel
    pthread_t listenerThread1, forwarderThread1;
    pthread_create(&listenerThread1, NULL, listenerThread, recvChannel1);
    pthread_create(&forwarderThread1, NULL, forwarderThread, sendChannel2);

    pthread_t listenerThread2, forwarderThread2;
    pthread_create(&listenerThread2, NULL, listenerThread, recvChannel2);
    pthread_create(&forwarderThread2, NULL, forwarderThread, sendChannel1);

    // Join threads (in this simple example, threads run indefinitely)
    pthread_join(listenerThread1, NULL);
    pthread_join(forwarderThread1, NULL);
    pthread_join(listenerThread2, NULL);
    pthread_join(forwarderThread2, NULL);

    return 0;
}