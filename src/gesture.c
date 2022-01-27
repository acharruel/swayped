#include <stdlib.h>
#include <stdio.h>

#include <libinput.h>

#include "gesture.h"

struct gesture {
	/*
	 * TODO:
	 * type
	 * handler for BEGIN, UPDATE and END
	 * to specialize for swipe, pinch...
	 */
};

struct gesture *gesture_new(struct libinput_event_gesture *li_gesture)
{
	struct gesture *gest;

	gest = calloc(1, sizeof(*gest));
	if (!gest)
		goto exit;

	switch (libinput_event_get_type(
			libinput_event_gesture_get_base_event(li_gesture))) {

	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		fprintf(stdout, " >>> %s: swipe BEGIN\n", __func__);
		break;

	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
		fprintf(stdout, " >>> %s: pinch BEGIN\n", __func__);
		break;

	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
		fprintf(stdout, " >>> %s: hold BEGIN\n", __func__);
		break;

	default:
		goto exit;
	};

	return gest;
exit:
	gesture_destroy(gest);
	return NULL;
}

void gesture_destroy(struct gesture *gest)
{
	if (!gest)
		return;

	fprintf(stdout, " >>> %s: gesture END\n", __func__);

	free(gest);
}
