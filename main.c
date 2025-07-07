#include "LeapC.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <conio.h>      // For _kbhit and _getch


int main() {
    LEAP_CONNECTION connection;
    if (LeapCreateConnection(NULL, &connection) != eLeapRS_Success) {
        printf("Failed to create connection\n");
        return -1;
    }
    if (LeapOpenConnection(connection) != eLeapRS_Success) {
        printf("Failed to open connection\n");
        return -1;
    }

    printf("Connection opened. Press 'q' to quit.\n");

    bool running = true;
    int frame_count = 0;

    while (running) {
        if (_kbhit()) {
            char key = _getch();
            if (key == 'q') {
                running = false;
                continue;
            }
        }

        LEAP_CONNECTION_MESSAGE msg;
        eLeapRS result = LeapPollConnection(connection, 100, &msg);
        if (result != eLeapRS_Success) {
            continue;
        }

        if (msg.type == eLeapEventType_Tracking) {
            frame_count++;

            // Only print every 1000 frames
            if (frame_count % 100 != 0) {
                continue;
            }

            const LEAP_TRACKING_EVENT* frame = msg.tracking_event;
            printf("\n[Frame #%d] Frame ID: %llu, Hands detected: %d\n",
                   frame_count, frame->info.frame_id, frame->nHands);

            for (int i = 0; i < frame->nHands; i++) {
                const LEAP_HAND* hand = &frame->pHands[i];
                printf("Hand %d (ID %llu, %s hand)\n",
                       i, hand->id,
                       (hand->type == eLeapHandType_Left) ? "Left" : "Right");

                for (int f = 0; f < 5; f++) {
                    const LEAP_DIGIT* finger = &hand->digits[f];
                    printf("  Finger %d: finger_id=%d, is_extended=%s\n",
                           f,
                           finger->finger_id,
                           finger->is_extended ? "true" : "false");
                }
            }
        }
    }

    LeapCloseConnection(connection);
    LeapDestroyConnection(connection);
    printf("Connection closed.\n");
    return 0;
}
