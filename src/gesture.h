#ifndef _GESTURE_H_
#define _GESTURE_H_

struct gesture;

struct gesture *gesture_new(struct libinput_event_gesture *li_gesture);
void gesture_destroy(struct gesture *gest);

#endif
