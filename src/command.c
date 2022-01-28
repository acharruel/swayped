#include <stdlib.h>
#include <string.h>

#include "sway/ipc-client.h"

#include "command.h"

enum sway_command {
	SWAY_CMD_WORKSPACE_PREV,
	SWAY_CMD_WORKSPACE_NEXT,
	SWAY_CMD_WORKSPACE_BACK_AND_FORTH
};

const char * const sway_command_str[] = {
	[SWAY_CMD_WORKSPACE_PREV]           = "workspace next",
	[SWAY_CMD_WORKSPACE_NEXT]           = "workspace prev",
	[SWAY_CMD_WORKSPACE_BACK_AND_FORTH] = "workspace back_and_forth",
};

void sway_send_command(enum sway_command cmd)
{
	char *socket_path = get_socketpath();
	uint32_t type = IPC_COMMAND;
	const char *command = sway_command_str[cmd];
	int socketfd = ipc_open_socket(socket_path);
	struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
	ipc_set_recv_timeout(socketfd, timeout);
	uint32_t len = strlen(command);
	ipc_single_command(socketfd, type, command, &len);
}

void command_workspace_next(void)
{
	sway_send_command(SWAY_CMD_WORKSPACE_NEXT);
}

void command_workspace_prev(void)
{
	sway_send_command(SWAY_CMD_WORKSPACE_PREV);
}

void command_workspace_back_and_forth(void)
{
	sway_send_command(SWAY_CMD_WORKSPACE_BACK_AND_FORTH);
}
