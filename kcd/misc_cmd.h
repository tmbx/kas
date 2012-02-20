/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _MISC_CMD_H
#define _MISC_CMD_H

int kcd_misc_cmd_kws_prop_change(struct kcd_kws_cmd_exec_state *ces);
int kcd_misc_cmd_grant_kcd_ticket(struct kcd_kws_cmd_exec_state *ces);
int kcd_misc_cmd_chat_msg(struct kcd_kws_cmd_exec_state *ces);
int kcd_misc_cmd_kws_get_uurl(struct kcd_kws_cmd_exec_state *ces);
int kcd_misc_cmd_pb_accept_chat(struct kcd_kws_cmd_exec_state *ces);

#endif

