from kcd_client import *

from sqlalchemy.sql import and_

# from kwmo
from kwmo.model.kcd.kcd_kws_kfs_current_view import KcdKwsKfsCurrentView
from kwmo.model.kcd.kcd_pub_email_info import KcdPubEmailInfo
from kwmo.model.kcd.kcd_pub_email_recipient_info import KcdPubEmailRecipientInfo

from kfs_lib import KFSOperations, get_kfs_skurl_subject, KFS_ROOT_INODE_ID, KFS_ROOT_COMMIT_ID, KFS_DIR

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# TODO: KWMO KCD client , needs to be tested and remove redundant imports
class KcdClient(BaseKcdClient):

    # This method returns a request id for a kws.
    def get_next_skurl_req_id(self, workspace_id, user_id):
        # Build ANP arguments list.
        am = kanp.ANP_msg()
        # Workspace bound arguments
        am.add_u64(workspace_id)
        am.add_u64(int(time.time()))
        am.add_u32(KCD_KWS_LOGIN_TYPE_SECURE)
        am.add_u32(user_id)
        am.add_u32(0) # command minor
        am.add_bin("") # command

        # Do the query.
        ext_am, res_am = self.workspace_bound_request('get_next_skurl_req_id', am.get_payload())

        # Decode request ID from the resulting ANP message.
        req_id = ext_am.get_u64()

        return req_id

    def get_kcd_download_ticket(self, workspace_id, share_id, user_id):
        # Connect to KCD.
        self.connect()

        # Select workspace role.
        self.select_role(kanp.KANP_KCD_ROLE_WORKSPACE)

        # Connect to workspace.
        self.connect_workspace(workspace_id = int(workspace_id), 
            user_id = user_id, email_id = 'kwmo', password = self.conf.db_passwd)

        # Send the command to KCD.
        am = kanp.ANP_msg()
        am.add_u64(workspace_id)
        am.add_u32(share_id)
        payload = am.get_payload()
        self.send_command_header(kanp.KANP_CMD_KFS_DOWNLOAD_REQ, len(payload))
        self.write(payload)
        log.debug("Sent download request to KCD for workspace %i, share %i." % ( workspace_id, share_id ) )

        # Get command result.
        h, m = kanp.get_anpt_all(self)

        # Close KCD connection.
        self.close()

        # Handle result.
        if h.type != kanp.KANP_RES_KFS_DOWNLOAD_REQ:
            log.debug("Got result type %i ('%s'?)." % ( h.type, kanp.get_kanp_constant(h.type, 'KANP_RES') ) )
            assert h.type == kanp.KANP_RES_FAIL
            raise kanp.KANPFailure(m.get_u32(), m.get_str())
        ticket = m.get_bin()

        return ticket

    # This method fetches details associated to an email id.
    def pubws_get_email_info(self, workspace_id, eid):
        # Get email info.
        email_info = KcdPubEmailInfo.get_by(kws_id=workspace_id, email_id=eid)
        if not email_info: return None

        # Convert email info.
        em = kbase.PropStore()
        em.id = email_info.email_id
        em.subject = email_info.subject
        em.date = email_info.date
        em.name = email_info.name
        em.email = email_info.address
        em.attachment_nbr = email_info.nb_attachment
        em.att_expire_date = email_info.att_expire_date
        # We get that information with the state updater.
        # em.att_expire_flag = email_info.att_expire_flag

        log.debug("Email info: workspace_id: '%s', eid: '%s', details: '%s'" % \
            ( str(workspace_id), str(eid), str(em.to_dict()) ))

        return em

    # This method fetches recipients associated to an email id.
    def pubws_get_eid_recipient_identities(self, workspace_id, eid):
        # Get recipients.
        recipients = KcdPubEmailRecipientInfo.query.filter(and_(KcdPubEmailRecipientInfo.kws_id == workspace_id, 
            KcdPubEmailRecipientInfo.email_id == eid)).all()
        if not recipients: return None

        # Convert recipients.
        r_list = []
        for recipient in recipients:    
            r = kbase.PropStore()
            r.name = recipient.name
            r.email = recipient.address
            r_list.append(r)

        log.debug("Recipients of email '%s': '%s'" % \
            ( str(eid), str(r_list) ))

        return r_list

    # This method requests to chat with the workspace creator.
    def pubws_chat_request(self, workspace_id, user_id, compat_v2, subject, timeout_seconds):
        # Get next skurl request ID from the KCD.
        request_id = self.get_next_skurl_req_id(workspace_id, user_id)

        # Build ANP arguments list.
        am = kanp.ANP_msg()
        # Workspace bound arguments
        am.add_u64(workspace_id)
        am.add_u64(int(time.time()))
        am.add_u32(KCD_KWS_LOGIN_TYPE_SECURE)
        am.add_u32(user_id)
        am.add_u32(0) # command minor
        am.add_bin("") # command
        # Other arguments
        am.add_u32(int(compat_v2))
        if compat_v2: am.add_u32(request_id)
        else: am.add_u64(request_id)
        am.add_u32(timeout_seconds)
        am.add_str(subject)

        # Post request to KCD database.
        ext_am, res_am = self.workspace_bound_request('pb_request_chat', am.get_payload())

        log.debug(
            "A chat request has been sent in public workspace '%s' by user id '%s'." % \
            ( str(workspace_id), str(user_id) ) )

        return request_id

    # This method fakes an accepted chat request (for development only).
    def pubws_chat_request_accept(self, workspace_id, user_id, kanp_minor, request_id):
        raise Exception("kcd_client pubws_chat_request_accept() has to be reimplemented.")

    # This method requests creator to create a workspace.
    def pubws_workspace_creation_request(self, workspace_id, user_id, compat_v2, subject):
        # Get next skurl request ID from KCD.
        request_id = self.get_next_skurl_req_id(workspace_id, user_id)

        # Build ANP arguments list.
        am = kanp.ANP_msg()
        # Workspace bound arguments
        am.add_u64(workspace_id)
        am.add_u64(int(time.time()))
        am.add_u32(KCD_KWS_LOGIN_TYPE_SECURE)
        am.add_u32(user_id)
        am.add_u32(0) # command minor
        am.add_bin("") # command
        # Other arguments
        am.add_u32(compat_v2)
        if compat_v2: am.add_u32(request_id)
        else: am.add_u64(request_id)
        am.add_str(subject)

        # Post request to KCD database.
        ext_am, res_am = self.workspace_bound_request('pb_request_workspace', am.get_payload())

        log.debug(
            "A request for workspace creation has been sent in public workspace '%s' by user id '%s'." % \
            ( str(workspace_id), str(user_id) ) )

        return request_id

    # This method creates a dir using KCD.
    def kfs_create_dir(self, workspace_id, email_id, share_id, user_id, parent_inode_id, parent_commit_id, name):

        log.debug("Creating KFS directory: workspace_id=%i, share_id=%i, user_id=%i, parent_inode_id=%i, name=%s" % \
            ( workspace_id, share_id, user_id, parent_inode_id, name ) )

        # Get upload ticket.
        ticket = self.get_kcd_upload_ticket(workspace_id, share_id, user_id)
        log.debug("An upload ticket has been created and has '%i' bytes." % ( len(ticket) ))

        # Connect to KCD.
        self.connect()

        # Select KFS role.
        self.select_role(kanp.KANP_KCD_ROLE_FILE_XFER)
        log.debug("kfs_create_dir(): selected kfs role.")

        # Create the directory.
        kfs_dir = kbase.PropStore()
        kfs_dir.kfs_op = kanp.KANP_KFS_OP_CREATE_DIR
        kfs_dir.parent_inode_id = parent_inode_id
        kfs_dir.parent_commit_id = parent_commit_id
        kfs_dir.name = name
        kfs_dir.kfs_error = None
        kfso = KFSOperations([kfs_dir], self, self)
        kfso.phase_one(email_id, ticket)

        if kfs_dir.kfs_error:
            raise Exception("Could not create directory '%s': '%s'." % ( kfs_dir.name, kfs_dir.kfs_error ) )

        # Close connection to KCD.
        self.close()

    # This method checks that, and creates, skurl directories for a user.
    def kcd_kfs_create_skurl_directories(self, workspace_id, share_id, user_id, user_email, email_id, 
                                     email_date, email_subject):
        dir_names = []
        dir_names += [ user_email ]
        dir_names += [ get_kfs_skurl_subject(email_date, email_subject) ]

        parent_inode_id = KFS_ROOT_INODE_ID
        parent_commit_id = KFS_ROOT_COMMIT_ID
        for dir_name in dir_names:
            dir = KcdKwsKfsCurrentView.get_by(kws_id=workspace_id, share_id=share_id, parent_inode=parent_inode_id, 
                inode_type=KFS_DIR, entry_name=dir_name)
            if dir:
                log.debug("Found directory '%s'." % ( dir_name ))
            else:
                log.debug("Could not find directory '%s'... creating it." % ( dir_name ))
                self.kfs_create_dir(workspace_id, email_id, share_id, user_id, parent_inode_id, parent_commit_id, dir_name)
                dir = KcdKwsKfsCurrentView.get_by(kws_id=workspace_id, share_id=share_id, parent_inode=parent_inode_id, 
                    inode_type=KFS_DIR, entry_name=dir_name)
                if not dir:
                    log.debug("Directory '%s' was not created." % ( dir_name ))
                    raise Exception("kfs_create_skurl_directories(): directory '%s' was not created." % \
                        ( dir_name ) )
                log.debug("Directory '%s' created." % ( dir_name ))
            parent_inode_id = dir.inode
            parent_commit_id = dir.commit_id

        # Return latest created directory.
        return dir

    # This method uploads file(s) to KCD.
    def kfs_upload(self, workspace_id, share_id, user_id, kfs_files):

        # Get upload ticket.
        ticket = self.get_kcd_upload_ticket(workspace_id, share_id, user_id)
        log.debug("An upload ticket has been created and has '%i' bytes." % ( len(ticket) ))

        # Create the directory.
        kfso = KFSOperations(kfs_files, self, self)
        kfso.phase_one(ticket)
        kfso.prepare_phase_two()
        kfso.phase_two()

    # Post a chat message.
    def post_chat_msg_request(self, workspace_id, channel_id, user_id, message):
        # Build the ANP command.
        cmd = kanp.ANP_msg()
        cmd.add_u64(workspace_id)
        cmd.add_u32(channel_id)
        cmd.add_str(message)

        # Build ANP arguments list.
        am = kanp.ANP_msg()
        # Workspace bound arguments
        am.add_u64(workspace_id)
        am.add_u64(int(time.time()))
        am.add_u32(KCD_KWS_LOGIN_TYPE_SECURE)
        am.add_u32(user_id)
        am.add_u32(kanp.KANP_MINOR) # command minor
        am.add_bin(cmd.get_payload()) # command

        # Post chat message to KCD database.
        ext_am, res_am = self.workspace_bound_request('cmd_chat_msg', am.get_payload())

        log.debug("Chat messages posted: workspace_id=%i, user_id=%i." % \
            ( workspace_id, user_id ) )

