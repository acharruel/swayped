#ifndef _GESTURE_H_
#define _GESTURE_H_

#include <libinput.h>

struct gesture;

struct gesture *gesture_new(struct libinput_event_gesture *li_gesture);
void gesture_destroy(struct gesture *gest,
		     struct libinput_event_gesture *li_gesture);
int gesture_update(struct gesture *gest,
		   struct libinput_event_gesture *li_gesture);

void gesture_set_data(struct gesture *gest, const void *data);
const void *gesture_get_data(struct gesture *gest);

/* gestures operations */
struct gesture_ops {
	int (*begin)(struct gesture *gest,
		     struct libinput_event_gesture *li_gesture);
	int (*update)(struct gesture *gest,
		      struct libinput_event_gesture *li_gesture);
	int (*end)(struct gesture *gest,
		   struct libinput_event_gesture *li_gesture);
};

/* export gestures operations */
struct gesture_ops *swipe_get_ops(void);

#endif
