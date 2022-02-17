#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <json.h>

#include "json_object.h"
#include "sway/ipc-client.h"

#include "command.h"

enum sway_command {
	SWAY_CMD_WORKSPACE_PREV,
	SWAY_CMD_WORKSPACE_NEXT,
	SWAY_CMD_WORKSPACE_BACK_AND_FORTH
};

const char * const sway_command_str[] = {
	[SWAY_CMD_WORKSPACE_PREV]           = "workspace prev",
	[SWAY_CMD_WORKSPACE_NEXT]           = "workspace next",
	[SWAY_CMD_WORKSPACE_BACK_AND_FORTH] = "workspace back_and_forth",
};

static char *sway_send_command(uint32_t type, const char *command)
{
	char *socket_path = get_socketpath();
	int socketfd = ipc_open_socket(socket_path);
	struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
	ipc_set_recv_timeout(socketfd, timeout);
	uint32_t len = strlen(command);
	return ipc_single_command(socketfd, type, command, &len);
}

static void sway_send_enum_command(enum sway_command cmd)
{
	char *resp = sway_send_command(IPC_COMMAND, sway_command_str[cmd]);
	/* release unused response from sway */
	free(resp);
}

void command_workspace_next(void)
{
	sway_send_enum_command(SWAY_CMD_WORKSPACE_NEXT);
}

void command_workspace_prev(void)
{
	sway_send_enum_command(SWAY_CMD_WORKSPACE_PREV);
}

void command_workspace_back_and_forth(void)
{
	sway_send_enum_command(SWAY_CMD_WORKSPACE_BACK_AND_FORTH);
}

void command_workspace_new(void)
{
	struct json_tokener *tok = NULL;
	struct json_object *jobj = NULL;
	struct json_object *json_workspace = NULL;
	struct json_object *json_workspace_num = NULL;
	struct json_object *json_workspace_focus = NULL;

	int i, ret;
	json_bool json_ret;
	bool focused;
	int workspace, max_workspace = 0;
	char *cmd = NULL;
	char *resp1 = NULL, *resp2 = NULL;

	resp1 = sway_send_command(IPC_GET_WORKSPACES, "");
	if (!resp1) {
		syslog(LOG_ERR, "Failed to get workspaces from sway");
		goto exit;
	}

	tok = json_tokener_new_ex(JSON_MAX_DEPTH);
	if (!tok) {
		syslog(LOG_ERR, "Failed to allocate json tokener");
		goto exit;
	}

	jobj = json_tokener_parse_ex(tok, resp1, -1);
	if (!jobj) {
		syslog(LOG_ERR, "Failed to parse json string");
		goto exit;
	}

	if (json_object_get_type(jobj) != json_type_array) {
		syslog(LOG_ERR,
		       "%s: Wrong JSON object type (%d), expect an array",
		       __func__, json_object_get_type(jobj));
		goto exit;
	}

	for (i = 0; i < json_object_array_length(jobj); i++) {
		json_workspace = json_object_array_get_idx(jobj, i);
		if (!json_workspace)
			continue;

		json_ret = json_object_object_get_ex(json_workspace, "num", &json_workspace_num);
		if (!json_ret)
			continue;

		json_ret = json_object_object_get_ex(json_workspace, "focused", &json_workspace_focus);
		if (!json_ret)
			continue;

		workspace = json_object_get_int(json_workspace_num);
		if (workspace > max_workspace)
			max_workspace = workspace;

		focused = json_object_get_boolean(json_workspace_focus);
		if (focused)
			syslog(LOG_INFO, "Workspace focused = %d", workspace);
	}

	if (max_workspace == 0)
		goto exit;

	ret = asprintf(&cmd, "workspace %d", max_workspace + 1);
	if (ret < 0)
		goto exit;

	resp2 = sway_send_command(IPC_COMMAND, cmd);

exit:
	json_tokener_free(tok);
	free(resp1);
	free(resp2);
	free(cmd);
}
