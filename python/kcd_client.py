# This library contains code for communication with a KCD.

# from system
import os, time, struct, random, pgdb, ConfigParser, logging
import psycopg2 as PgSQL

# local
from kcdpg import KCD_KWS_LOGIN_TYPE_SECURE
from kcd_lib import get_kcd_db_conn

# from kpython
import kbase
import kpg
from kpg import ntos

# from kas-python
import kanp
import tcp_client

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

class BaseKcdClient(tcp_client.TcpClient):

    # Constructor
    # parameters:
    #  conf: configuration object, or None
    def __init__(self, conf, db_conn=None):
        # Get config object.
        self.conf = conf

        # Get db connection if provited.
        self.db_conn = db_conn

        # Command ID.
        self.command_id = 0

        # Initialize parent class, but do not connect.
        tcp_client.TcpClient.__init__(self, self.conf.kcd_host, self.conf.kcd_port, use_openssl=True, allow_dh=True)

    # Connect to KCD.
    def connect(self):
        # Reset command ID.
        self.command_id = 0

        # Connect to KCD.
        tcp_client.TcpClient.connect(self)

    # Send command header to KCD.
    def send_command_header(self, type, payload_length):
        self.command_id += 1
        log.debug("Sending command %i to KCD." % ( self.command_id ) )
        kanp.send_anpt_header(self, type, self.command_id, payload_length)

    # This method connects or reuse a DB connection to KCD.
    def db_connect(self):
        if not self.db_conn: self.db_conn = get_kcd_db_conn(self.conf)

    # Do a workspace-bound KCD database query.
    def workspace_bound_request(self, stored_proc_name, args_payload):
        # Connect or reuse DB connection.
        self.db_connect()

        # Do the query.
        query = "SELECT %s('%s')" % ( stored_proc_name, pgdb.escape_bytea(args_payload) )
        cur = kpg.exec_pg_query_rb_on_except(self.db_conn, query)
        row = cur.fetchone()
        self.db_conn.commit()

        # Handle result.
        am = kanp.ANP_msg()
        am.parse(row[0].value)

        # Get generic error status.
        error_status = am.get_u32()

        if error_status == -1:
            # Generic error.
            err_msg = am.get_str()
            raise Exception("Error: '%s'." % ( err_msg ) )

        else:
            # No generic error... continue parsing.

            # Get result type.
            res_type = am.get_u32()
            res_buf = am.get_bin()
            res_error = am.get_u32()

            if res_type == kanp.KANP_RES_OK:
                # Success
                ext_buf_am = am
                res_buf_am = kanp.ANP_msg()
                res_buf_am.parse(res_buf)
                return ext_buf_am, res_buf_am

            else:
                # Specific error
                am = kanp.ANP_msg()
                am.parse(res_buf)
                err_code = am.get_u32()
                err_msg = am.get_str()
                raise Exception("Error: '%s' stored procedure failed: error: %i ('%s')" % \
                    ( stored_proc_name, err_code, err_msg ) )

 
    # This method selects a KCD role.
    def select_role(self, role):
        log.debug("select_role() called")

        # send request
        m = kanp.ANP_msg()
        m.add_u32(role)
        payload = m.get_payload()
        self.send_command_header(kanp.KANP_CMD_MGT_SELECT_ROLE, len(payload))
        self.write(payload)

        # get response
        h, m = kanp.get_anpt_all(self)
        if h.type != kanp.KANP_RES_OK:
            assert h.type == kanp.KANP_RES_FAIL
            raise kanp.KANPFailure(m.get_u32(), m.get_str())
        assert h.type == kanp.KANP_RES_OK
        log.debug("Role %i selected." % ( role ))

    # Connect to a workspace.
    def connect_workspace(self, workspace_id, last_evt_id=0, last_evt_timestamp=0, 
            user_id=0, real_name='', email_address='', email_id='', ticket='', password='',
            delete_on_login=0):

        # Create ANP command.
        m = kanp.ANP_msg()
        m.add_u64(workspace_id)
        m.add_u32(delete_on_login)
        m.add_u64(last_evt_id)
        m.add_u64(last_evt_timestamp)
        m.add_u32(user_id)
        m.add_str(real_name)
        m.add_str(email_address)
        m.add_str(email_id)
        m.add_bin(ticket)
        m.add_str(password)
        
        # Send the command to KCD.
        payload = m.get_payload()
        self.send_command_header(kanp.KANP_CMD_KWS_CONNECT_KWS, len(payload))
        log.debug("Sent anp command to connect to workspace %i." % ( workspace_id ) )
        self.write(payload)
        # Get command result.
        h, m = kanp.get_anpt_all(self)
        if h.type != kanp.KANP_RES_KWS_CONNECT_KWS:
            assert h.type == kanp.KANP_RES_FAIL
            raise kanp.KANPFailure(m.get_u32(), m.get_str())

        # Decode result.
        result = m.get_u32()
        error = m.get_str()
        user_id = m.get_u32()
        email_id = m.get_str()
        last_evt_id = m.get_u64()
        secure = m.get_u32()
        password_assigned = m.get_u32()
        kwmo_server = m.get_str()
        if result != kanp.KANP_KWS_LOGIN_OK:
            raise kanp.KANPFailure(result, error)

        log.debug("Connected to workspace %i." % ( workspace_id ) )

    # Send invitation.
    def send_invitation(self, workspace_id, message, invitees):
        # Create command.
        m = kanp.ANP_msg()
        m.add_u64(workspace_id)
        m.add_str(message)
        m.add_u32(len(invitees))
        for invitee in invitees:
            m.add_str(invitee.real_name)
            m.add_str(invitee.email_address)
            m.add_u64(invitee.key_id)
            m.add_str(invitee.org_name)
            m.add_str(invitee.password)
            m.add_u32(int(invitee.send_mail))

        # Send the command to KCD.
        payload = m.get_payload()
        self.send_command_header(kanp.KANP_CMD_KWS_INVITE_KWS, len(payload))
        log.debug("Sent anp invite command for workspace %i." % ( workspace_id ) )
        self.write(payload)
        
        # Get command result.
        h, m = kanp.get_anpt_all(self)
        if h.type != kanp.KANP_RES_KWS_INVITE_KWS:
            log.debug("Received type '%s'." % ( str(h.type) ) )
            assert h.type == kanp.KANP_RES_FAIL
            raise kanp.KANPFailure(m.get_u32(), m.get_str())

        # Decode the result.
        ws_url = m.get_str()
        invitees_nb = m.get_u32()
        for i in range(0, invitees_nb):
            invitees[i].email_id = m.get_str()
            invitees[i].url = m.get_str()
            invitees[i].error = m.get_str()

        return ws_url, invitees

    # This method sends a download notification to KCD.
    def send_download_notification(self, workspace_id, user_id, share_id, inode_id, commit_id, pubws_email_id, 
            login_type=KCD_KWS_LOGIN_TYPE_SECURE, minor=0, cmd=""):

        # Build ANP arguments list.
        am = kanp.ANP_msg()
        # Workspace bound arguments
        am.add_u64(workspace_id)
        am.add_u64(int(time.time()))
        am.add_u32(login_type)
        am.add_u32(user_id)
        am.add_u32(minor)
        am.add_bin(cmd)
        # Other arguments
        am.add_u32(share_id)
        am.add_u64(inode_id)
        am.add_u64(commit_id)
        am.add_u64(pubws_email_id)

        # Send notification to KCD database.
        ext_am, res_am = self.workspace_bound_request('notify_file_download', am.get_payload())

        log.debug("Download notification posted for workspace %i." % \
            ( workspace_id ) )

    # Save notification policy.    
    def save_notification_policy(self, workspace_id, user_id, notification_policy):
        # Connect to database if needed.
        self.db_connect()

        # Build ANP arguments list.
        am = kanp.ANP_msg()
        am.add_u64(int(workspace_id))
        am.add_u32(int(user_id))
        am.add_u32(notification_policy)

        # Do the query.
        query = "SELECT do_notif_mgt(E'%s')" % ( pgdb.escape_bytea(am.get_payload()) )
        cur = kpg.exec_pg_query_rb_on_except(self.db_conn, query)
        row = cur.fetchone()
        self.db_conn.commit()        

        return 0

    # Invite user(s).
    def invite_users(self, workspace_id, message, invitees, email_id='kwmo'):
        # Connect to KCD.
        self.connect()

        # Select workspace role.
        self.select_role(kanp.KANP_KCD_ROLE_WORKSPACE)

        # Connect to workspace.
        self.connect_workspace(workspace_id = int(workspace_id), email_id = email_id, password = self.conf.db_passwd)

        # Send the invitation.
        ws_url, invitees = self.send_invitation(int(workspace_id), message, invitees)

        # Close connection to KCD.
        self.close()

        return ws_url, invitees

    def get_kcd_upload_ticket(self, workspace_id, share_id, user_id, email_id='kwmo'):
        # Connect to KCD.
        self.connect()

        # Select workspace role.
        self.select_role(kanp.KANP_KCD_ROLE_WORKSPACE)

        # Connect to workspace.
        self.connect_workspace(workspace_id = int(workspace_id),
            user_id = user_id, email_id = email_id, password = self.conf.db_passwd)

        # Send the command to KCD.
        am = kanp.ANP_msg()
        am.add_u64(workspace_id)
        am.add_u32(share_id)
        payload = am.get_payload()
        self.send_command_header(kanp.KANP_CMD_KFS_UPLOAD_REQ, len(payload))
        self.write(payload)
        log.debug("Sent upload request to KCD for workspace %i, share %i." % ( workspace_id, share_id ) )

        # Get command result.
        h, m = kanp.get_anpt_all(self)

        # Close KCD connection.
        self.close()

        # Handle result.
        if h.type != kanp.KANP_RES_KFS_UPLOAD_REQ:
            log.debug("Got result type '%s'." % ( kanp.get_kanp_constant(h.type, 'KANP_') ) )
            assert h.type == kanp.KANP_RES_FAIL
            raise kanp.KANPFailure(m.get_u32(), m.get_str())
        ticket = m.get_bin()

        return ticket

    # This method deletes KFS entries using KCD.
    # The <kfs_entries> must be a list of KFSOPDirDelete and KFSOPFileDelete objects.
    def kfs_delete_entries(self, workspace_id, share_id, user_id, email_id, kfs_entries):

        log.debug("Deleting KFS entries:  workspace_id=%i, share_id=%i, user_id=%i, kfs_entries=%s" % \
            ( workspace_id, share_id, user_id, str(kfs_entries) ) )

        # Get upload ticket.
        ticket = self.get_kcd_upload_ticket(workspace_id, share_id, user_id)
        log.debug("kfs_delete_entries(): an upload ticket has been created and has '%i' bytes." % ( len(ticket) ))

        # Connect to KCD.
        self.connect()

        # Select KFS role.
        self.select_role(kanp.KANP_KCD_ROLE_FILE_XFER)
        log.debug("kfs_delete_entries(): selected kfs role.")

        # Delete entries.
        # FIXME kcd_client imports kfs_lib that imports kcd_client ...
        import kfs_lib
        kfso = kfs_lib.KFSOperations(kfs_entries, self, self)
        kfso.phase_one(email_id, ticket)

        # Check for errors.
        entries_in_error = []
        for kfs_entry in kfs_entries:
            if kfs_entry.kfs_error:
                entries_in_error.append((kfs_entry.inode_id, kfs_entry.commit_id, kfs_entry.kfs_error))
        if len(entries_in_error) > 0:
            raise Exception(("Could not delete KFS entries: workspace_id=%i, share_id=%i, user_id=%i, email_id=%s,"+\
                             " (inode_id, commit_id, err) list: %s.") \
                                % ( workspace_id, share_id, user_id, str(email_id), str(entries_in_error) ) )

        # Close connection to KCD.
        self.close()

