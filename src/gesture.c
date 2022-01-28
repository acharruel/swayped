#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "gesture.h"

enum gesture_type {
	HOLD,
	PINCH,
	SWIPE
};

struct gesture {
	/*
	 * TODO:
	 * type
	 * handler for BEGIN, UPDATE and END
	 * to specialize for swipe, pinch...
	 */
	enum gesture_type type;
	struct gesture_ops *ops;
	const void *data;
};

struct gesture *gesture_new(struct libinput_event_gesture *li_gesture)
{
	struct gesture *gest;
	int ret = 0;

	gest = calloc(1, sizeof(*gest));
	if (!gest)
		goto exit;

	switch (libinput_event_get_type(
			libinput_event_gesture_get_base_event(li_gesture))) {

	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		syslog(LOG_DEBUG, "%s: swipe BEGIN\n", __func__);
		gest->type = SWIPE;
		gest->ops = swipe_get_ops();
		break;

	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
		syslog(LOG_DEBUG, "%s: pinch BEGIN\n", __func__);
		gest->type = PINCH;
		break;

	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
		syslog(LOG_DEBUG, "%s: hold BEGIN\n", __func__);
		gest->type = HOLD;
		break;

	default:
		goto exit;
	};

	if (gest->ops && gest->ops->begin)
		ret = gest->ops->begin(gest, li_gesture);
	if (ret < 0) {
		syslog(LOG_ERR, "Failed to execute gesture BEGIN operation\n");
		goto exit;
	}

	return gest;
exit:
	gesture_destroy(gest, li_gesture);
	return NULL;
}

void gesture_destroy(struct gesture *gest,
		     struct libinput_event_gesture *li_gesture)
{
	int ret = 0;

	if (!gest)
		return;

	if (gest->ops && gest->ops->end)
		ret = gest->ops->end(gest, li_gesture);
	if (ret < 0)
		syslog(LOG_ERR, "Failed to execute gesture END operation\n");

	switch (gest->type) {

	case SWIPE:
		syslog(LOG_DEBUG, "%s: swipe END\n", __func__);
		break;

	case PINCH:
		syslog(LOG_DEBUG, "%s: pinch END\n", __func__);
		break;

	case HOLD:
		syslog(LOG_DEBUG, "%s: hold END\n", __func__);
		break;

	default:
		break;
	};

	free(gest);
}

int gesture_update(struct gesture *gest,
		   struct libinput_event_gesture *li_gesture)
{
	int ret = 0;

	if (gest->ops && gest->ops->update)
		ret = gest->ops->update(gest, li_gesture);
	if (ret < 0) {
		syslog(LOG_ERR, "Failed to execute gesture UPDATE operation\n");
		goto exit;
	}

exit:
	return ret;
}

void gesture_set_data(struct gesture *gest, const void *data)
{
	gest->data = data;
}

const void *gesture_get_data(struct gesture *gest)
{
	return gest ? gest->data : NULL;
}
