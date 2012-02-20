/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _MGT_H
#define _MGT_H

int kcd_mgt_create_kws(struct kcd_kws_cmd_exec_state *ces);
int kcd_mgt_freemium_confirm(struct kcd_kws_cmd_exec_state *ces);
int kcd_mgt_connect_kws(struct kcd_kws_cmd_exec_state *ces);
int kcd_mgt_disconnect_kws(struct kcd_kws_cmd_exec_state *ces);
int kcd_mgt_invite_kws(struct kcd_kws_cmd_exec_state *ces);

#endif

