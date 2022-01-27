#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libinput.h>

#include <libudev.h>

enum {
	LIBINPUT_FD,
	NB_FDS
};

struct context {
	struct udev *udev;
	struct libinput *li;
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
	[LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS] = "POINTER_SCROLL_CONTINUOUS",
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

	libinput_unref(ctx->li);
	udev_unref(ctx->udev);

	free(ctx);
}

static struct context *context_new(void)
{
	struct context *ctx = NULL;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		goto exit;

	ctx->udev = udev_new();
	if (!ctx->udev) {
		fprintf(stderr, "Failed to create udev context\n");
		goto exit;
	}

	ctx->li = libinput_udev_create_context(&interface, ctx, ctx->udev);
	if (!ctx->li) {
		fprintf(stderr, "Failed to create libinput context\n");
		goto exit;
	}

	libinput_udev_assign_seat(ctx->li, "seat0");
	/* libinput_log_set_priority(ctx->li, LIBINPUT_LOG_PRIORITY_INFO); */

	return ctx;
exit:
	context_destroy(ctx);
	return NULL;
}

#if 0
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
#endif

static bool event_is_swipe(struct libinput_event *event)
{
	enum libinput_event_type type = libinput_event_get_type(event);
	switch (type) {
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
		return true;
	default:
		return false;
	};
}

static int process_gesture(struct libinput_event_gesture *gesture)
{
	int ret = 0;
	enum libinput_event_type type;
	int nfingers;
	bool canceled;
	double dx, dy;

	type = libinput_event_get_type(libinput_event_gesture_get_base_event(gesture));
	nfingers = libinput_event_gesture_get_finger_count(gesture);
	canceled = !!libinput_event_gesture_get_cancelled(gesture);
	dx = libinput_event_gesture_get_dx(gesture);
	dy = libinput_event_gesture_get_dy(gesture);

	fprintf(stdout, " >>> %s: type '%s'\n", __func__, event_to_str[type]);
	fprintf(stdout, " >>> %s: fingers %d\n", __func__, nfingers);
	fprintf(stdout, " >>> %s: canceled %d\n", __func__, canceled);
	fprintf(stdout, " >>> %s: dx %f dy %f\n", __func__, dx, dy);

	return ret;
}

static int process_events(struct libinput *li)
{
	int ret = 0;
	struct libinput_event *event = NULL;
	struct libinput_event_gesture *gesture = NULL;

	do {
		event = libinput_get_event(li);
		if (!event)
			break;

		if (!event_is_swipe(event))
			continue;

		gesture = libinput_event_get_gesture_event(event);
		if (!gesture)
			continue;

		ret = process_gesture(gesture);
		if (ret < 0) {
			libinput_event_destroy(event);
			goto exit;
		}

		libinput_event_destroy(event);
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

	do {
		do {
			ret = poll(fds, NB_FDS, -1);
		} while (ret == -1 && errno == EINTR);

		if (fds[LIBINPUT_FD].revents) {
			libinput_dispatch(ctx->li);
			process_events(ctx->li);
		}

	} while (1);

exit:
	context_destroy(ctx);
	return ret;
}
