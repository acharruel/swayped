#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syslog.h>

#include "command.h"
#include "gesture.h"

#define SWIPE_DIST_THRESHOLD	100.0
#define OBLIQUE_RATIO		(tan(M_PI / 8))

enum swipe_direction {
	SWIPE_UP,
	SWIPE_DOWN,
	SWIPE_LEFT,
	SWIPE_RIGHT
};

struct swipe {
	double dx;
	double dy;
	int nfingers;
};

static void swipe_detected(struct swipe *sw, enum swipe_direction direction)
{
	switch (direction) {
	case SWIPE_UP:
		syslog(LOG_INFO, "%s: UP fingers %d\n", __func__, sw->nfingers);
		if (sw->nfingers == 3)
			command_workspace_new();
		break;
	case SWIPE_DOWN:
		syslog(LOG_INFO, "%s: DOWN finger %d\n", __func__, sw->nfingers);
		if (sw->nfingers == 3)
			command_workspace_back_and_forth();
		break;
	case SWIPE_LEFT:
		syslog(LOG_INFO, "%s: LEFT fingers %d\n", __func__, sw->nfingers);
		if (sw->nfingers == 3)
			command_workspace_prev();
		break;
	case SWIPE_RIGHT:
		syslog(LOG_INFO, "%s: RIGHT fingers %d\n", __func__, sw->nfingers);
		if (sw->nfingers == 3)
			command_workspace_next();
		break;
	default:
		break;
	};
}

static int swipe_begin(struct gesture *gest,
		       struct libinput_event_gesture *li_gesture)
{
	int ret = 0;
	struct swipe *sw = NULL;

	sw = calloc(1, sizeof(*sw));
	if (!sw) {
		ret = -ENOMEM;
		goto exit;
	}

	sw->nfingers = libinput_event_gesture_get_finger_count(li_gesture);

	gesture_set_data(gest, sw);
exit:
	return ret;
}

static int swipe_update(struct gesture *gest,
			struct libinput_event_gesture *li_gesture)
{
	int ret = 0;
	struct swipe *sw = (struct swipe *)gesture_get_data(gest);

	sw->dx += libinput_event_gesture_get_dx(li_gesture);
	sw->dy += libinput_event_gesture_get_dy(li_gesture);

	return ret;
}

static int swipe_end(struct gesture *gest,
		     struct libinput_event_gesture *li_gesture)
{
	int ret = 0;
	struct swipe *sw = (struct swipe *)gesture_get_data(gest);
	double dx_abs = fabs(sw->dx);
	double dy_abs = fabs(sw->dy);

	syslog(LOG_DEBUG, "%s: dx %f dy %f\n", __func__, sw->dx, sw->dy);

	if (dx_abs >= SWIPE_DIST_THRESHOLD &&
	    dy_abs >= SWIPE_DIST_THRESHOLD) {
		if ((dx_abs / dy_abs) > (dy_abs / dx_abs + OBLIQUE_RATIO)) {
			/* horizontal swipe */
			swipe_detected(sw, sw->dx > 0 ? SWIPE_RIGHT : SWIPE_LEFT);
		} else if ((dy_abs / dx_abs) > (dx_abs / dy_abs + OBLIQUE_RATIO)) {
			/* vertical swipe */
			swipe_detected(sw, sw->dy > 0 ? SWIPE_DOWN : SWIPE_UP);
		}
	} else if (dx_abs > SWIPE_DIST_THRESHOLD) {
		swipe_detected(sw, sw->dx > 0 ? SWIPE_RIGHT : SWIPE_LEFT);
	} else if (dy_abs > SWIPE_DIST_THRESHOLD) {
		swipe_detected(sw, sw->dy > 0 ? SWIPE_DOWN : SWIPE_UP);
	}

	free(sw);
	return ret;
}

static struct gesture_ops swipe_ops = {
	.begin  = swipe_begin,
	.update = swipe_update,
	.end    = swipe_end,
};

struct gesture_ops *swipe_get_ops(void)
{
	return &swipe_ops;
}
