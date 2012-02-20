/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

#ifndef _KANP_CORE_DEFS_H
#define _KANP_CORE_DEFS_H

/* Current KANP version. */
#define KANP_MAJOR_VERSION   	        0u
#define KANP_MINOR_VERSION   	        6u

/* Version history:
 * 1: 2008-2009: Initial version.
 * 2: mars 2009: Added ID of the last event available on the KCD on login.
 * 3: may 2009 : Reworked the login process. Added a call to set the password
 *               of a user. Modified KFS phase 1 and GET_UURL fields.
 * 4: nov 2009:  Added user management calls. Modified login process. Added
 *               transient events. Added thin KFS flag.
 * 5: nov 2009:  Added error code in VNC end session and the freemium call.
 * 6: feb 2010:  Added expiration delay in GET_UURL. Added email ID in GET_UURL result.
 */
 
/* Compatibility notes:
 *
 * The version of an event is set to the mininum version required to fully
 * describe the data or operation associated to the event. This allows us to
 * support old KWMs until someone come and "poison" the workspace with newer
 * events.
 */

/* Workspace login types and user IDs:
 * 
 * There are four login types for a user in a workspace: root, kwmo, normal and
 * secure. The 'root' and 'kwmo' login types are said to be privileged because
 * they can bypass most permission checks if required. When the 'kwmo' login
 * type is used no workspace events are sent.
 * 
 * The 'normal' and 'secure' login types are set when a user logs in with normal
 * or secure credentials.
 * 
 * The 'root' and 'kwmo' login types may be used either with user ID 0 or any
 * other exisiting user ID. If a non-zero user ID is 0, all permission checks
 * are performed using the permissions of that user.
 */
 
/* Command permission level checks:
 *
 * When a client is modifying the data of a workspace:
 * - If the workspace is frozen and the user is not admin:
 *   - The command is rejected.
 * - If the workspace is deeply frozen and the user has not root permissions:
 *   - The command is rejected.
 *
 * When a client is modifying the properties of a workspace:
 * - If the client privilege level is lower than the command privilege level:
 *   - The command is rejected.
 * 
 * When a client is modifying the properties of a target user:
 * - If the target has ID 0 or the target does not exist:
 *   - The command is rejected.
 * - If the target is the client:
 *   - If the command can be applied to self:
 *     - The command is accepted. No further checks are made.
 *   - Otherwise:
 *     - The command is rejected.
 * - If the client privilege level is lower than the command privilege level:
 *   - The command is rejected.
 * - If the client privilege level is lower than the target privilege level:
 *   - The command is rejected.
 */
 

/* Last compatible minor version supported. */
#define KANP_LAST_COMP_MINOR_VERSION    1u

/* KANP type format (32 bits):
 *
 * 31-28: protocol type (01).
 * 27-26: role:
 *        - 0: Command.
 *        - 1: Result.
 *        - 2: Event.
 *        - 3: Reserved.
 * 25-16: namespace ID:
 *        - 0: Generic.
 *        - 1: Workspace management.
 *        - 2: Workspace-specific.
 *        - 3: Reserved.
 *        - 4: Chat.
 *        - 5: File transfer.
 *        - 6: Application sharing.
 *        - 7: Whiteboard.
 *
 * 15-08: subtype.
 * 07-00: reserved.
 */

#define KANP_PROTO       	    	(1 << 28)

#define KANP_CMD	    	    	(0 << 26)
#define KANP_RES 	    	    	(1 << 26)
#define KANP_EVT  	    	    	(2 << 26)

#define KANP_NS_GEN     	    	(0 << 16)
#define KANP_NS_MGT     	    	(1 << 16)
#define KANP_NS_KWS     	    	(2 << 16)
#define KANP_NS_CHAT     	    	(4 << 16)
#define KANP_NS_KFS     	    	(5 << 16)
#define KANP_NS_VNC			(6 << 16)
#define KANP_NS_WB			(7 << 16)
#define KANP_NS_PB			(8 << 16)


/* Generic results: */

/* Command successful. */
#define KANP_RES_OK     	    	(KANP_PROTO | KANP_RES | KANP_NS_GEN | (0 << 8))

/* Command failed.
 *   UINT32 Failure type.
 *   STR    Failure explanation.
 *   <Failure specific data>.
 */
#define KANP_RES_FAIL     	    	(KANP_PROTO | KANP_RES | KANP_NS_GEN | (1 << 8))


/* Type of failures. */

/* Generic. Only the error message indicates what the error is. */
#define KANP_RES_FAIL_GEN                   0

/* Backend error: KCD has encountered an internal error. It will die as soon as
 * it has managed to push this message. The ID of the message is 0.
 */
#define KANP_RES_FAIL_BACKEND               1

/* Connect failed because the user was not uniquely identified. The list of
 * candidates follow. Only used in compatibility mode.
 *   UINT32 Number of candidates.
 *     UINT32 User ID.
 *     STR    User name
 *     STR    User email address.
 */
#define KANP_RES_FAIL_CHOOSE_USER_ID        2

/* Connect failed because the view of the user is not consistent, probably
 * because the server crashed and lost some events. All events must be refetched
 * from the server. Only used in version <= 2.
 */
#define KANP_RES_FAIL_EVT_OUT_OF_SYNC       3

/* Role negociation failed because the client is too old. */
#define KANP_RES_FAIL_MUST_UPGRADE          4

/* Action failed because the user does not have the permission required. */
#define KANP_RES_FAIL_PERM_DENIED           5

/* File quota exceeded. */
#define KANP_RES_FAIL_FILE_QUOTA_EXCEEDED   6

/* Resource quota exceeded. */
#define KANP_RES_FAIL_RESOURCE_QUOTA        7


/* Resource quota specific error codes. */

/* Miscellaneous resource quota error. */
#define KANP_RESOURCE_QUOTA_GENERAL         0          

/* Secure operation denied. */
#define KANP_RESOURCE_QUOTA_NO_SECURE       1


/* Select the role of KCD.
 *   UINT32 Role.
 */
#define KANP_CMD_MGT_SELECT_ROLE	(KANP_PROTO | KANP_CMD | KANP_NS_MGT | (0 << 8))
#define KANP_KCD_ROLE_WORKSPACE		1
#define KANP_KCD_ROLE_FILE_XFER		2
#define KANP_KCD_ROLE_APP_SHARE		3


/* Create a workspace.
 *   STR    Name of the workspace.
 *   BIN    Ticket.
 *   UINT32 Public workspace flag.
 *   UINT32 Secure flag (added in version 3).
 *   UINT32 Thin KFS flag (added in version 4).
 */
#define KANP_CMD_MGT_CREATE_KWS     	(KANP_PROTO | KANP_CMD | KANP_NS_MGT | (1 << 8))

/* Workspace created.
 *   UINT64 Workspace ID.
 *   BIN    Nonce (removed in version 3).
 *   STR    Email ID (added in version 3).
 */
#define KANP_RES_MGT_KWS_CREATED    	(KANP_PROTO | KANP_RES | KANP_NS_MGT | (1 << 8))

/* Send the Freemium confirmation email to the user specified.
 *   STR    Root password.
 *   STR    Email address.
 *   STR    Confirmation link.
 */
#define KANP_CMD_MGT_FREEMIUM_CONFIRM   (KANP_PROTO | KANP_CMD | KANP_NS_MGT | (2 << 8))

/* Invite people in a workspace.
 *
 * Version <= 2:
 *   UINT64 Workspace ID.
 *   UINT32 Number of people invited.
 *     STR    User real name.
 *     STR    User email address.
 *     UINT32 Is the user a member?
 *     UINT32 Is the user an administrator?
 *     If member:
 *        UINT64 Key ID.
 *        STR    Organization name.
 *     Else:
 *        STR    Decryption password.
 *    
 * Version >= 3:
 *   UINT64 Workspace ID.
 *   STR    Invitation message, if an invitation email must be sent by the KCD.
 *   UINT32 Number of people invited.
 *     STR    User real name.
 *     STR    User email address.
 *     UINT64 Key ID. 0 if none.
 *     STR    Organization name. Empty if none.
 *     STR    Password. Empty if none.
 *     UINT32 True if an invitation email must be sent by the KCD.
 */
#define KANP_CMD_KWS_INVITE_KWS    	(KANP_PROTO | KANP_CMD | KANP_NS_KWS | (2 << 8))

/* Invite accepted.
 * 
 * Version <= 2:
 *   BIN    Nonce.
 *   STR    Web link.
 * 
 * Version >= 3:
 *   STR    Workspace-linked email URL.
 *   UINT32 Number of people invited.
 *     STR    Email ID.
 *     STR    Invitation URL.
 *     STR    If the invitation email could not be sent, this string describes
 *            the error, otherwise it is empty.
 */
#define KANP_RES_KWS_INVITE_KWS    	(KANP_PROTO | KANP_RES | KANP_NS_KWS | (2 << 8))


/* Login result codes. */

/* The login credentials are accepted. */
#define KANP_KWS_LOGIN_OK                       1

/* The credentials are accepted but the login failed since the information
 * about the last event received is invalid, probably because the server
 * crashed and lost some events. All events must be refetched from the server.
 */
#define KANP_KWS_LOGIN_OOS                      2

/* The password and/or the ticket are refused. */
#define KANP_KWS_LOGIN_BAD_PWD_OR_TICKET        3

/* The workspace ID is invalid. */
#define KANP_KWS_LOGIN_BAD_KWS_ID               4

/* The email ID is invalid or it has been purged from the database. */
#define KANP_KWS_LOGIN_BAD_EMAIL_ID             5

/* The workspace has been deleted. */
#define KANP_KWS_LOGIN_DELETED_KWS              6

/* The user account has been locked. */
#define KANP_KWS_LOGIN_ACCOUNT_LOCKED           7

/* The user has been banned. */
#define KANP_KWS_LOGIN_BANNED                   8


/* Connect to a workspace.
 * 
 * Version <= 2:
 *   UINT64 Workspace ID.
 *   UINT64 ID of the last event received for this workspace.
 *   UINT64 Timestamp of the last event received, or 0 if none.
 *   UINT32 User ID, if known, otherwise 0.
 *   STR    User real name.
 *   STR    User email address.
 *   BIN    Nonce.
 *   UINT32 Is the user a member?
 *     If member:
 *       BIN    Ticket.
 *     Else:
 *       STR    Decryption password.
 *
 * Version >= 3:
 *   UINT64 Workspace ID.
 *   UINT32 True if the workspace must be deleted on successful login
 *          (version 4).
 *   UINT64 ID of the last event received for this workspace.
 *   UINT64 Timestamp of the last event received, or 0 if none.
 *   UINT32 User ID, if known, otherwise 0.
 *   STR    User real name.
 *   STR    User email address.
 *   STR    Email ID. Empty if not known in compatibility mode.
 *   BIN    Ticket.
 *   STR    Password.
 */
#define KANP_CMD_KWS_CONNECT_KWS    	(KANP_PROTO | KANP_CMD | KANP_NS_KWS | (3 << 8))

/* Connected to the workspace.
 * 
 * Version <= 2:
 *   UINT32 User ID.
 *   UINT64 ID of the last event available on the KCD (version 2).
 * 
 * Version >= 3:
 *   UINT32 Result code.
 *   STR    Error message.
 *   // The remaining information is provided in all cases but it is only
 *   // meaningful if the result code is KANP_KWS_LOGIN_OK, KANP_KWS_LOGIN_OOS
 *   // or KANP_KWS_LOGIN_BAD_PWD_OR_TICKET.
 *   UINT32 User ID.
 *   STR    Email ID (for v2 compatibility, set when result is KANP_KWS_LOGIN_OK).
 *   UINT64 ID of the last event available on the KCD.
 *   UINT32 True if the workspace is secure.
 *   UINT32 True if a password has been assigned to the user.
 *   STR    Address of the KWMO server.
 */
#define KANP_RES_KWS_CONNECT_KWS	(KANP_PROTO | KANP_RES | KANP_NS_KWS | (3 << 8))


/* Disconnect from a workspace.
 *   UINT64 Workspace ID.
 */
#define KANP_CMD_KWS_DISCONNECT_KWS    	(KANP_PROTO | KANP_CMD | KANP_NS_KWS | (4 << 8))


/* Get a unique URL representing an email.
 *   UINT64 Workspace ID.
 *   STR    From name.
 *   STR    From email address.
 *   STR    Subject.
 *   UINT32 Number of attachments (added in version 3).
 *   UINT32 Number of recipients.
 *   UINT32 Number of items per recipient.
 *     STR    Recipient name.
 *     STR    Recipient email address.
 *   UINT64 Expiration delay in seconds, 0 is infinite (added in version 6).
 */
#define KANP_CMD_KWS_GET_UURL		(KANP_PROTO | KANP_CMD | KANP_NS_KWS | (5 << 8))

/* A unique url for a mail.
 *   STR    UURL.
 *   UINT64 Date (added in version 3).
 *   UINT64 Email ID (added in version 6).
 */
#define KANP_RES_KWS_UURL		(KANP_PROTO | KANP_RES | KANP_NS_KWS | (5 << 8))


/* Set the password of a user, clobbering the previous one if needed. Added in
 * version 3. Permission check: self/manager.
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   STR    Password.
 */
#define KANP_CMD_KWS_SET_USER_PWD       (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (6 << 8))

/* All the commands that set a workspace property or workspace user property are
 * returned this result on success. Added in version 4.
 *   UINT64 Generated event ID, or 0 if none.
 */
#define KANP_RES_KWS_PROP_CHANGE        (KANP_PROTO | KANP_RES | KANP_NS_KWS | (6 << 8))

/* Set the name of a user, clobbering the previous one if needed. Added in
 * version 4. Permission check: self/manager.
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   STR    Name.
 */
#define KANP_CMD_KWS_SET_USER_NAME      (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (7 << 8))

/* Set the value of the administrator flag of the user specified. Added in
 * version 4. Permission check: privileged.
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_USER_ADMIN     (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (8 << 8))

/* Set the value of the manager flag of the user specified. Added in version 4.
 * Permission check: admin.
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_USER_MANAGER   (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (9 << 8))

/* Set the value of the lock flag of the user specified. Added in version 4.
 * Permission check: manager.
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_USER_LOCK      (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (10 << 8))

/* Set the value of the ban flag of the user specified. Added in version 4.
 * Permission check: manager.
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_USER_BAN       (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (11 << 8))

/* Set the name of the workspace specified. Added in version 4. Permission
 * check: admin.
 *   UINT64 Workspace ID.
 *   STR    Name.
 */
#define KANP_CMD_KWS_SET_NAME           (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (12 << 8))

/* Set the value of the secure flag of the workspace specified. Added in version
 * 4. Permission check: admin.
 *   UINT64 Workspace ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_SECURE         (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (13 << 8))

/* Set the value of the freeze flag of the workspace specified. Added in version
 * 4. Permission check: admin.
 *   UINT64 Workspace ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_FREEZE         (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (14 << 8))

/* Set the value of the deep freeze flag of the workspace specified. Added in
 * version 4. Permission check: privileged.
 *   UINT64 Workspace ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_DEEP_FREEZE    (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (15 << 8))

/* Set the value of the thin KFS flag of the workspace specified. Added in
 * version 4. Permission check: admin.
 *   UINT64 Workspace ID.
 *   UINT32 Flag value.
 */
#define KANP_CMD_KWS_SET_THIN_KFS       (KANP_PROTO | KANP_CMD | KANP_NS_KWS | (16 << 8))


/* Send a chat message.
 *   UINT64 Workspace ID.
 *   UINT32 Chat ID.
 *   STR    Chat message.
 */
#define KANP_CMD_CHAT_MSG		(KANP_PROTO | KANP_CMD | KANP_NS_CHAT | (1 << 8))


/* KFS entry type identifiers. */
#define KANP_KFS_ENTRY_FILE             1
#define KANP_KFS_ENTRY_DIR              2

/* KFS operation identifiers. */
#define KANP_KFS_OP_CREATE_FILE         1
#define KANP_KFS_OP_CREATE_DIR          2
#define KANP_KFS_OP_UPDATE_FILE         3
#define KANP_KFS_OP_DELETE_FILE         4
#define KANP_KFS_OP_DELETE_DIR          5
#define KANP_KFS_OP_MOVE_FILE           6
#define KANP_KFS_OP_MOVE_DIR            7

/* KFS submessage identifiers. */
#define KANP_KFS_SUBMESSAGE_FILE        1
#define KANP_KFS_SUBMESSAGE_CHUNK       2
#define KANP_KFS_SUBMESSAGE_COMMIT      3
#define KANP_KFS_SUBMESSAGE_ABORT       4

/* Obtain a ticket to download files from a share.
 *   UINT64 Workspace ID.
 *   UINT32 KFS share ID.
 */
#define KANP_CMD_KFS_DOWNLOAD_REQ	(KANP_PROTO | KANP_CMD | KANP_NS_KFS | (1 << 8))

/* Download ticket granted.
 *   BIN    Download ticket.
 */
#define KANP_RES_KFS_DOWNLOAD_REQ	(KANP_PROTO | KANP_RES | KANP_NS_KFS | (1 << 8))

/* Obtain a ticket to upload files to the share.
 *   UINT64 Workspace ID.
 *   UINT32 KFS share ID.
 */
#define KANP_CMD_KFS_UPLOAD_REQ		(KANP_PROTO | KANP_CMD | KANP_NS_KFS | (2 << 8))

/* Upload ticket granted.
 *   BIN    Upload ticket.
 */
#define KANP_RES_KFS_UPLOAD_REQ		(KANP_PROTO | KANP_RES | KANP_NS_KFS | (2 << 8))

/* Download files from the share.
 *   BIN    Download ticket.
 *   UINT32 Number of files to download.
 *     UINT64 Inode to download.
 *     UINT64 Offset in the file (to support resumption).
 *     UINT64 Inode commit ID.
 */
#define KANP_CMD_KFS_DOWNLOAD_DATA	(KANP_PROTO | KANP_CMD | KANP_NS_KFS | (3 << 8))

/* Result of the command above. Several messages of this type may be returned,
 * until all the data has been transferred. To accelerate the download speed,
 * this message contains a series of submessages.
 *   UINT32 Number of submessages.
 *     <Submessage>
 *
 * Submessage "file": this submessage announces the data of the next file.
 *   UINT32 Number of elements in this message (ignored).
 *   UINT32 Submessage type ("file").
 *   UINT64 File size.
 *   UINT64 Amount of data the server will send. If this is 0, no 'chunk'
 *          submessage follows.
 *
 * Submessage "chunk": this submessage is sent to send a chunk of the file
 * currently being downloaded.
 *   UINT32 Number of elements in this message (ignored).
 *   UINT32 Submessage type ("chunk").
 *   BIN    Chunk data.
 */
#define KANP_RES_KFS_DOWNLOAD_DATA	(KANP_PROTO | KANP_RES | KANP_NS_KFS | (3 << 8))

/* Describe the changes to apply to the share (phase 1).
 *   BIN    Upload ticket.
 *   UINT64 Public email ID. 0 if none.
 *   UINT32 Number of changes.
 *     <change>
 *   
 * Create file / dir:
 *   UINT32 Number of elements in this message (ignored).
 *   UINT32 Change type ("create file" / "create dir").
 *   UINT64 Parent inode.
 *   UINT64 Parent commit ID.
 *   STR    Entry name/path.
 *
 * Update file:
 *   UINT32 Number of elements in this message (ignored).
 *   UINT32 Change type ("update file").
 *   UINT64 Inode to update.
 *   UINT64 Inode commit ID.
 *
 * Delete file / dir:
 *   UINT32 Number of elements in this message (ignored).
 *   UINT32 Change type ("delete file" / "delete dir")
 *   UINT64 Inode to delete.
 *   UINT64 Inode commit ID.
 *
 * Move file / dir:
 *   UINT32 Number of elements in this message (ignored).
 *   UINT32 Change type ("move file" / "move directory").
 *   UINT64 Inode to move.
 *   UINT64 Inode commit ID.
 *   UINT64 Parent inode.
 *   UINT64 Parent commit ID.
 *   STR    Move entry name/path.
 */
#define KANP_CMD_KFS_PHASE_1	        (KANP_PROTO | KANP_CMD | KANP_NS_KFS | (4 << 8))

/* Result of the command above.
 *   UINT64 Commit ID.
 *   UINT32 Number of changes.
 *     UINT32 Result (failed => 0, ok => 1).
 *     STR    Reason of failure, if any.
 */
#define KANP_RES_KFS_PHASE_1	        (KANP_PROTO | KANP_RES | KANP_NS_KFS | (4 << 8))

/* Upload the content of the files (phase 2). To accelerate the upload speed,
 * this message contains a series of submessages. One RES_OK reply is sent for
 * each command of this type and a final RES_OK message is sent when all the
 * files have been transferred.
 *   UINT32 Number of submessages.
 *     <Submessage>
 *
 * Submessage "chunk": this submessage is sent to upload a chunk of the
 * file currently being uploaded.
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Submessage type ("chunk").
 *   BIN    Chunk data.
 *
 * Submessage "commit": this submessage is sent to commit the data of the
 * file currently being uploaded.
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Submessage type ("commit").
 *   BIN    File hash (checked for sanity).
 *
 * Submessage "abort": this submessage is sent to abort the transfer of
 * the file being uploaded. It can be sent before or between the
 * transfer of the chunks.
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Submessage type ("abort").
 */
#define KANP_CMD_KFS_PHASE_2	        (KANP_PROTO | KANP_CMD | KANP_NS_KFS | (5 << 8))


/* Obtain a ticket to reconnect to the KCD in server-side application sharing
 * mode.
 *   UINT64 Workspace ID.
 */
#define KANP_CMD_VNC_START_TICKET	(KANP_PROTO | KANP_CMD | KANP_NS_VNC | (1 << 8))

/* Server-side application ticket granted. 
 *   BIN    Server-side application sharing ticket.
 */
#define KANP_RES_VNC_START_TICKET	(KANP_PROTO | KANP_RES | KANP_NS_VNC | (1 << 8))

/* Start the application sharing session.
 *   BIN    Server-side application sharing ticket.
 *   STR    Subject.
 */
#define KANP_CMD_VNC_START_SESSION	(KANP_PROTO | KANP_CMD | KANP_NS_VNC | (2 << 8))

/* Application sharing session started (added in version 3).
 *   UINT64 Session ID.
 */
#define KANP_RES_VNC_START_SESSION	(KANP_PROTO | KANP_RES | KANP_NS_VNC | (2 << 8))

/* Obtain a ticket to reconnect to the KCD in client-side application sharing
 * mode.
 *   UINT64 Workspace ID.
 *   UINT64 Session ID.
 */
#define KANP_CMD_VNC_CONNECT_TICKET	(KANP_PROTO | KANP_CMD | KANP_NS_VNC | (3 << 8))

/* Client-side application ticket granted. 
 *   BIN    Client-side application sharing ticket.
 */
#define KANP_RES_VNC_CONNECT_TICKET	(KANP_PROTO | KANP_RES | KANP_NS_VNC | (3 << 8))

/* Connect to the application sharing session.
 *   BIN    Client-side application sharing ticket.
 */
#define KANP_CMD_VNC_CONNECT_SESSION	(KANP_PROTO | KANP_CMD | KANP_NS_VNC | (4 << 8))


/* Send a whiteboard modification.
 *   UINT62 Workspace ID.
 *   UINT32 Whiteboard ID.
 *   UINT32 Type (0 -> new drawing (form), 1 -> transformation).
 *   STR    XML string.
 */
#define KANP_CMD_WB_DRAW		(KANP_PROTO | KANP_CMD | KANP_NS_WB | (1 << 8))
 
/* Clear the entire whiteboard.
 *   UINT62 Workspace ID.
 *   UINT32 Whiteboard ID.
 */
#define KANP_CMD_WB_CLEAR		(KANP_PROTO | KANP_CMD | KANP_NS_WB | (2 << 8))


/* Accept a requested chat in a public workspace.
 *   UINT64 Workspace ID.
 *   UINT32 Request ID. (version <= 2)
 *   UINT64 Request ID. (version >= 3)
 *   UINT32 User ID.
 *   UINT32 Channel ID.
 */
#define KANP_CMD_PB_ACCEPT_CHAT		(KANP_PROTO | KANP_CMD | KANP_NS_PB | (1 << 8))


/* Workspace flags. */

/* True if the workspace is public. */
#define KANP_KWS_FLAG_PUBLIC            (1 << 0)

/* True if the workspace has been frozen by the workspace admin. */
#define KANP_KWS_FLAG_FREEZE            (1 << 1)

/* True if the workspace is frozen by the system admin. */
#define KANP_KWS_FLAG_DEEP_FREEZE       (1 << 2)

/* True if the data of the KFS files should be deleted when the files are
 * deleted by the user.
 */
#define KANP_KWS_FLAG_THIN_KFS          (1 << 3)

/* True if the workspace is marked secure. */
#define KANP_KWS_FLAG_SECURE            (1 << 4)

/* True if the workspace has been deleted. Unpublished. */
#define KANP_KWS_FLAG_DELETE            (1 << 30)

/* True if the workspace is in V2 compatibility mode. Unpublished. */
#define KANP_KWS_FLAG_COMPAT_V2         (1 << 29)


/* User flags. */

/* The user is a workspace administrator. */
#define KANP_USER_FLAG_ADMIN            (1 << 0)

/* The user is a workspace manager. */
#define KANP_USER_FLAG_MANAGER          (1 << 1)

/* The user has been registered to the workspace. */
#define KANP_USER_FLAG_REGISTER         (1 << 2)

/* The user's account has been locked. */
#define KANP_USER_FLAG_LOCK             (1 << 3)

/* The user has been banned. */
#define KANP_USER_FLAG_BAN              (1 << 4)

/* The user has root permission. Unpublished. */
#define KANP_USER_FLAG_ROOT             (1 << 30)


/* Workspace and user property types. */

/* Name of the workspace. */
#define KANP_PROP_KWS_NAME              1

/* Flags of the workspace. */
#define KANP_PROP_KWS_FLAGS             2

/* Name given to the user by the administrator. */
#define KANP_PROP_USER_NAME_ADMIN       101

/* Name given by the user himself. */
#define KANP_PROP_USER_NAME_USER        102

/* Flags of the user. */
#define KANP_PROP_USER_FLAGS            103


/* A workspace has been created.
 *
 * Version <= 2:
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID of the creator.
 *   STR    User real name.
 *   STR    User email address.
 *   UINT32 Is the user a member?
 *   UINT32 Is the user an admin?
 *   STR    Organization name.
 *
 * Version >= 3: 
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID of the creator.
 *   STR    User real name.
 *   STR    User email address.
 *   STR    Organization name.
 *   STR    Name of the workspace.
 *   UINT32 Workspace flags.
 *   STR    Address of the KWMO.
 */
#define KANP_EVT_KWS_CREATED    	(KANP_PROTO | KANP_EVT | KANP_NS_KWS | (1 << 8))

/* Some users have been invited to a workspace.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID of the user inviting (added in version 3).
 *   UINT32 Number of users invited.
 *     UINT32 User ID of the user invited.
 *     STR    User real name.
 *     STR    User email address.
 *     UINT32 Is the user a member? (removed in version 3).
 *     UINT32 Is the user an admin? (removed in version 3).
 *     STR    Organization name, if the user is a member.
 */
#define KANP_EVT_KWS_INVITED    	(KANP_PROTO | KANP_EVT | KANP_NS_KWS | (2 << 8))

/* A user has joined a workspace for the first time.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   STR    User specified name.
 */
#define KANP_EVT_KWS_USER_REGISTERED   	(KANP_PROTO | KANP_EVT | KANP_NS_KWS | (3 << 8))

/* A workspace has been deleted. This event is deprecated since version 4.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 */
#define KANP_EVT_KWS_DELETED            (KANP_PROTO | KANP_EVT | KANP_NS_KWS | (4 << 8))

/* Transient event. The user has been logged out of the workspace at the request
 * of the server.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 Login status code corresponding to the error. 0 if none.
 *   STR    Error message.
 */
#define KANP_EVT_KWS_LOG_OUT            (KANP_PROTO | KANP_EVT | KANP_NS_KWS | (5 << 8))

/* Some properties of the workspace have been modified.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT32 Number of changes.
 *     UINT32 Property type.
 *     <Property data>
 *
 * Set workspace name:
 *   STR    Name.
 *
 * Set workspace flags:
 *   UINT32 Flags.
 *
 * Set user admin/user name:
 *   UINT32 User ID.
 *   STR    Name.
 *
 * Set user flags:
 *   UINT32 User ID.
 *   UINT32 Flags.
 */
#define KANP_EVT_KWS_PROP_CHANGE        (KANP_PROTO | KANP_EVT | KANP_NS_KWS | (6 << 8))


/* Chat message received.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 Chat ID.
 *   UINT32 User ID.
 *   STR    Chat message.
 */
#define KANP_EVT_CHAT_MSG	   	(KANP_PROTO | KANP_EVT | KANP_NS_CHAT | (1 << 8))


/* Changes to the share have been applied (phase 1).
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT32 KFS share ID.
 *   UINT64 Commit ID.
 *   UINT32 Number of changes.
 *     <changes>
 *
 * Create file / dir:
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Change type ("create file" / "create dir").
 *   UINT64 Inode created.
 *   UINT64 Parent inode.
 *   STR    Entry name.
 *
 * Update file:
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Change type ("update file").
 *   UINT64 Inode updated.
 *
 * Delete file / dir:
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Change type ("delete file" / "delete dir")
 *   UINT64 Inode deleted.
 *
 * Move file / dir:
 *   UINT32 Number of elements in this message (for compatibility).
 *   UINT32 Change type ("move file" / "move directory").
 *   UINT64 Inode moved.
 *   UINT64 New parent inode.
 *   STR    New entry name.
 */
#define KANP_EVT_KFS_PHASE_1    	(KANP_PROTO | KANP_EVT | KANP_NS_KFS | (1 << 8))

/* Files have been uploaded (phase 2).
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT32 KFS share ID.
 *   UINT64 Commit ID (same as in KANP_EVT_KFS_PHASE_1).
 *   UINT32 Number of files uploaded (same order as in KANP_EVT_KFS_PHASE_1).
 *     UINT64 Inode.
 *     UINT64 File size.
 *     BIN    File hash.
 */
#define KANP_EVT_KFS_PHASE_2    	(KANP_PROTO | KANP_EVT | KANP_NS_KFS | (2 << 8))

/* A file has been downloaded.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT32 KFS share ID.
 *   UINT64 KFS inode.
 *   UINT64 KFS commit ID.
 */
#define KANP_EVT_KFS_DOWNLOAD (KANP_PROTO | KANP_EVT | KANP_NS_KFS | (3 << 8))


/* Application sharing session started.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT64 Session ID.
 *   STR    Subject.
 */
#define KANP_EVT_VNC_START		(KANP_PROTO | KANP_EVT | KANP_NS_VNC | (1 << 8))

/* Application sharing session ended.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT64 Session ID.
 *   UINT32 Error code (added in version 5).
 *   STR    Error message (added in version 5).
 */
#define KANP_EVT_VNC_END		(KANP_PROTO | KANP_EVT | KANP_NS_VNC | (2 << 8))


/* New whiteboard drawing to display.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT32 Whiteboard ID.
 *   UInt32 Type (see KANP_CMD_WB_DRAW).
 *   STR    XML string.
 */
#define KANP_EVT_WB_DRAW		(KANP_PROTO | KANP_EVT | KANP_NS_WB | (1 << 8))

/* Clear whiteboard.
 *   UINT64 Workspace ID.
 *   UINT64 Date (seconds since UNIX epoch).
 *   UINT32 User ID.
 *   UINT32 Whiteboard ID.
 */
#define KANP_EVT_WB_CLEAR		(KANP_PROTO | KANP_EVT | KANP_NS_WB | (2 << 8))


/* A chat is triggered.
 *   UINT64 Workspace ID.
 *   UINT64 Date.
 *   UINT32 Request ID. (version <= 2)
 *   UINT64 Request ID. (version >= 3)
 *   UINT32 User ID.
 *   STR    Subject.
 *   UINT32 Timeout in seconds.
 */
#define KANP_EVT_PB_TRIGGER_CHAT	(KANP_PROTO | KANP_EVT | KANP_NS_PB | (1 << 8))

/* A chat is accepted.
 *   UINT64 Workspace ID.
 *   UINT64 Date.
 *   UINT32 Request ID. (version <= 2)
 *   UINT64 Request ID. (version >= 3)
 *   UINT32 User ID.
 *   UINT32 Channel ID.
 */
#define KANP_EVT_PB_CHAT_ACCEPTED	(KANP_PROTO | KANP_EVT | KANP_NS_PB | (2 << 8))

/* A workspace creation is triggered.
 *   UINT64 Workspace ID.
 *   UINT64 Date.
 *   UINT32 Request ID. (version <= 2)
 *   UINT64 Request ID. (version >= 3)
 *   UINT32 User ID.
 *   STR    Subject.
 */
#define KANP_EVT_PB_TRIGGER_KWS		(KANP_PROTO | KANP_EVT | KANP_NS_PB | (3 << 8))


/* These flags control whether email notifications and email summaries are sent
 * to the user.
 */
#define KANP_EMAIL_NOTIF_FLAG           1
#define KANP_EMAIL_SUMMARY_FLAG         2

/* These values are used to describe notification management requests. */
#define KANP_ENABLE_KWS_NOTIF           1
#define KANP_DISABLE_KWS_NOTIF          2
#define KANP_DISABLE_ALL_NOTIF          3 /* Unimplemented. */


/* Format of an event:
 * The first element is the workspace ID, or 0 if none.
 * The second element is the date at which the event was generated.
 */

/* There are two types of events, permanent events and transient events.
 * Permanent events have a unique, non-null message ID, while transient events
 * have a null message ID.
 */

/* Format of a workspace ticket:
 * STR    User real name.
 * STR    User email address.
 * STR    KAS host.
 * UINT32 KAS port.
 * UINT64 Key ID of the KPS of the user.
 */

/* Format of a KCD ticket (ANP format):
 * UINT32 Ticket type.
 * UINT64 Workspace ID.
 * UINT32 Login type.
 * UINT32 User ID.
 * Extra data.
 * BIN 16 bytes nonce.
 */

/* Format of a KCD download ticket payload:
 * UINT32 KFS share ID.
 */

/* Format of a KCD upload ticket payload:
 * UINT32 KFS share ID.
 */
 
/* Format of a KCD server-side application sharing payload:
 * No extra data.
 */
 
/* Format of a KCD client-side application sharing payload:
 * UINT64 Session ID.
 */

/* Format of a notification payload in the workspace notification log:
 *
 * Chat message sent:
 *   STR    Message.
 *
 * File uploaded:
 *   UINT64 Public email ID. 0 if none.
 *   UINT32 Number of files uploaded.
 *     UINT32 True if this is a file creation.
 *     STR    Share path to the file.
 *
 * File downloaded:
 *   UINT64 Public email ID. 0 if none.
 *   STR    Share path to the file.
 *
 * VNC session started:
 *   STR    Subject.
 */

/* Format of a notification management request:
 *   UINT64 Workspace ID. 0 if none.
 *   STR    Email address of the user.
 *   UINT32 Notification command.
 *   UINT32 Summary command.
 */
 
#define KANP_KCD_TICKET_DOWNLOAD		1
#define KANP_KCD_TICKET_UPLOAD			2
#define KANP_KCD_TICKET_VNC_SERVER		3
#define KANP_KCD_TICKET_VNC_CLIENT		4

#endif

