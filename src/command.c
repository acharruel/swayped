#include <stdlib.h>

#include "command.h"

void command_workspace_next(void)
{
	system("swaymsg workspace next");
}

void command_workspace_prev(void)
{
	system("swaymsg workspace prev");
}

void command_workspace_back_and_forth(void)
{
	system("swaymsg workspace back_and_forth");
}
