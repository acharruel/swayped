#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/signalfd.h>

#include <libinput.h>

#include <libudev.h>

#include "gesture.h"

enum {
	LIBINPUT_FD,
	SIGNAL_FD,
	NB_FDS
};

struct context {
	/* process lifecycle */
	int sigfd;
	bool stop;

	/* libudev context */
	struct udev *udev;

	/* libinput context */
	struct libinput *li;

	/* hold current gesture pointer */
	struct gesture *gesture;
};

const char * const event_to_str[] = {
	[LIBINPUT_EVENT_NONE] = "NONE",
	[LIBINPUT_EVENT_DEVICE_ADDED] = "DEVICE_ADDED",
	[LIBINPUT_EVENT_DEVICE_REMOVED] = "DEVICE_REMOVED",
	[LIBINPUT_EVENT_KEYBOARD_KEY] = "KEYBOARD_KEY",
	[LIBINPUT_EVENT_POINTER_MOTION] = "POINTER_MOTION",
	[LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE] = "POINTER_MOTION_ABSOLUTE",
	[LIBINPUT_EVENT_POINTER_BUTTON] = "POINTER_BUTTON",
	[LIBINPUT_EVENT_POINTER_AXIS] = "POINTER_AXIS",
	[LIBINPUT_EVENT_POINTER_SCROLL_WHEEL] = "POINTER_SCROLL_WHEEL",
	[LIBINPUT_EVENT_POINTER_SCROLL_FINGER] = "POINTER_SCROLL_FINGER",
	[LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS] =
		"POINTER_SCROLL_CONTINUOUS",
	[LIBINPUT_EVENT_TOUCH_DOWN] = "TOUCH_DOWN",
	[LIBINPUT_EVENT_TOUCH_UP] = "TOUCH_UP",
	[LIBINPUT_EVENT_TOUCH_MOTION] = "TOUCH_MOTION",
	[LIBINPUT_EVENT_TOUCH_CANCEL] = "TOUCH_CANCEL",
	[LIBINPUT_EVENT_TOUCH_FRAME] = "TOUCH_FRAME",
	[LIBINPUT_EVENT_TABLET_TOOL_AXIS] = "TABLET_TOOL_AXIS",
	[LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY] = "TABLET_TOOL_PROXIMITY",
	[LIBINPUT_EVENT_TABLET_TOOL_TIP] = "TABLET_TOOL_TIP",
	[LIBINPUT_EVENT_TABLET_TOOL_BUTTON] = "TABLET_TOOL_BUTTON",
	[LIBINPUT_EVENT_TABLET_PAD_BUTTON] = "TABLET_PAD_BUTTON",
	[LIBINPUT_EVENT_TABLET_PAD_RING] = "TABLET_PAD_RING",
	[LIBINPUT_EVENT_TABLET_PAD_STRIP] = "TABLET_PAD_STRIP",
	[LIBINPUT_EVENT_TABLET_PAD_KEY] = "TABLET_PAD_KEY",
	[LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN] = "GESTURE_SWIPE_BEGIN",
	[LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE] = "GESTURE_SWIPE_UPDATE",
	[LIBINPUT_EVENT_GESTURE_SWIPE_END] = "GESTURE_SWIPE_END",
	[LIBINPUT_EVENT_GESTURE_PINCH_BEGIN] = "GESTURE_PINCH_BEGIN",
	[LIBINPUT_EVENT_GESTURE_PINCH_UPDATE] = "GESTURE_PINCH_UPDATE",
	[LIBINPUT_EVENT_GESTURE_PINCH_END] = "GESTURE_PINCH_END",
	[LIBINPUT_EVENT_GESTURE_HOLD_BEGIN] = "GESTURE_HOLD_BEGIN",
	[LIBINPUT_EVENT_GESTURE_HOLD_END] = "GESTURE_HOLD_END",
	[LIBINPUT_EVENT_SWITCH_TOGGLE] = "SWITCH_TOGGLE"
};

static int open_restricted(const char *path, int flags, void *user_data)
{
	int fd = open(path, flags);
	return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
	close(fd);
}

const static struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

static void context_destroy(struct context *ctx)
{
	if (!ctx)
		return;

	if (ctx->gesture)
		gesture_destroy(ctx->gesture, NULL);

	libinput_unref(ctx->li);
	udev_unref(ctx->udev);

	free(ctx);
}

static struct context *context_new(void)
{
	struct context *ctx = NULL;
	sigset_t mask;
	int ret;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		goto exit;

	ctx->udev = udev_new();
	if (!ctx->udev) {
		syslog(LOG_ERR, "Failed to create udev context\n");
		goto exit;
	}

	ctx->li = libinput_udev_create_context(&interface, ctx, ctx->udev);
	if (!ctx->li) {
		syslog(LOG_ERR, "Failed to create libinput context\n");
		goto exit;
	}

	ret = libinput_udev_assign_seat(ctx->li, "seat0");
	if (ret < 0) {
		syslog(LOG_ERR, "Failed to assign udev seat: %s\n",
			strerror(-ret));
		goto exit;
	}

	/* handle signals for clean termination */
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTERM);
	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret < 0) {
		syslog(LOG_ERR, "Failed to set sigprocmask: %s\n",
			strerror(-ret));
		goto exit;
	}

	ctx->sigfd = signalfd(-1, &mask, 0);
	if (ctx->sigfd < 0)
		goto exit;

	return ctx;
exit:
	context_destroy(ctx);
	return NULL;
}

static bool event_is_gesture(struct libinput_event *event)
{
	enum libinput_event_type type = libinput_event_get_type(event);
	switch (type) {
	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
	case LIBINPUT_EVENT_GESTURE_HOLD_END:
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
	case LIBINPUT_EVENT_GESTURE_PINCH_END:
	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
	case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
		return true;
	default:
		return false;
	};
}

static int event_process_gesture(struct libinput *li,
				 struct libinput_event *event)
{
	int ret = 0;
	struct context *ctx = libinput_get_user_data(li);
	enum libinput_event_type type;

	type = libinput_event_get_type(event);
	/* nfingers = libinput_event_gesture_get_finger_count(gesture); */
	/* canceled = !!libinput_event_gesture_get_cancelled(gesture); */

	switch (type) {
	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		if (ctx->gesture) {
			syslog(LOG_ERR, "Cancelling ongoing gesture\n");
			gesture_destroy(ctx->gesture,
				libinput_event_get_gesture_event(event));
		}
		ctx->gesture = gesture_new(
				libinput_event_get_gesture_event(event));
		if (!ctx->gesture) {
			ret = -ENOMEM;
			goto exit;
		}
		break;

	case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
		if (!ctx->gesture)
			syslog(LOG_ERR, "Missing ongoing gesture to update\n");
		gesture_update(ctx->gesture,
			       libinput_event_get_gesture_event(event));
		break;

	case LIBINPUT_EVENT_GESTURE_HOLD_END:
	case LIBINPUT_EVENT_GESTURE_PINCH_END:
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
		if (!ctx->gesture)
			syslog(LOG_ERR, "Missing ongoing gesture to end\n");
		gesture_destroy(ctx->gesture,
				libinput_event_get_gesture_event(event));
		ctx->gesture = NULL;
		break;

	default:
		break;
	};

exit:
	return ret;
}

static int event_process(struct libinput *li)
{
	int ret = 0;
	struct libinput_event *event = NULL;

	do {
		event = libinput_get_event(li);
		if (!event)
			break;

		if (!event_is_gesture(event))
			continue;

		ret = event_process_gesture(li, event);
		if (ret < 0) {
			libinput_event_destroy(event);
			goto exit;
		}

		libinput_event_destroy(event);
		/* loop to make sure we don't miss spurious event */
	} while (event);
exit:
	return ret;
}

int main(void)
{
	int ret = EXIT_SUCCESS;
	struct context *ctx = NULL;
	struct pollfd fds[NB_FDS];

	ctx = context_new();
	if (!ctx) {
		ret = EXIT_FAILURE;
		goto exit;
	}

	fds[LIBINPUT_FD].fd = libinput_get_fd(ctx->li);
	fds[LIBINPUT_FD].events = POLLIN;

	fds[SIGNAL_FD].fd = ctx->sigfd;
	fds[SIGNAL_FD].events = POLLIN;

	do {
		do {
			ret = poll(fds, NB_FDS, -1);
		} while (ret == -1 && errno == EINTR);

		if (fds[LIBINPUT_FD].revents) {
			libinput_dispatch(ctx->li);
			event_process(ctx->li);
		}

		/* signals */
		if (fds[SIGNAL_FD].revents) {
			syslog(LOG_ERR, "Signal received, bailing out...\n");
			ctx->stop = 1;
		}

	} while (!ctx->stop);

exit:
	context_destroy(ctx);
	return ret;
}
