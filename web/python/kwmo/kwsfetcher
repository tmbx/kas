#!/usr/bin/env python

# from system
import sys, site, os, time, re, select, struct, traceback, socket, getopt, logging, logging.config

# from kpython
from kpg import exec_pg_query_rb_on_except, exec_pg_select_rb, ntos
from kfile import first_existing_file

# Paster configuration file.
paster_config_path = first_existing_file([
    '/usr/share/teambox/web/kwmo/production.ini',
    'production.ini',
    'development.ini'])

# Path for storing kwsfetcher information.
#fetcher_path = "/var/teambox/kwsfetcher/workspaces"
fetcher_path = "/tmp/teambox"

# Keepalive parameters. See the Linux documentation for details. We want the
# connection to time out after 6 minutes.
tcp_keepalive_time = 60
tcp_keepalive_intvl = 60
tcp_keepalive_probes = 5

# from system
import sys, os, time, re, select, struct, traceback, socket, getopt, logging, logging.config

# Append kwmo application path to python path.
sys.path.append('/usr/share/teambox/web/kwmo')

# Add site directory (would work with PYTHONPATH but not with sys.path because sys.path does not 
# handle PTH files).
site.addsitedir('/usr/share/teambox/pylons_env/lib/python' + sys.version[:3] + '/site-packages')

# from system or kpylons
from sqlalchemy.sql import and_
from paste.deploy import appconfig
from pylons import config

# from kwmo
from kcd_lib import get_kcd_external_conf_object, get_kcd_db_conn
import kfs_lib
from kanp import *

# Current postgres connection.
kcd_db_conn = None

# Dictionary mapping workspace IDs to workspaces.
kws_dict = {}

# List of workspaces to delete.
kws_delete_list = []

# Highest workspace ID received.
last_kws_id = 0
min_kws_id = None
max_kws_id = None

# True if the list of workspaces needs to be polled.
poll_kws_list_flag = 1

# Load models globally with a custom paster configuration.
def load_models_as_globals(paster_config_path, globals, locals):
    from kwmo.config.environment import load_environment
    relative_to = None
    if not paster_config_path.startswith('/'):
        relative_to = os.getcwd()
    conf = appconfig('config:' + paster_config_path, relative_to=relative_to)
    load_environment(conf.global_conf, conf.local_conf)

    global SASession, Workspace, User, ChatMessage, KfsNode, ChatRequest, VncSession, WSRequest

    # Import SQLAlchemy session and models in global module namespace.
    import kwmo.model
    SASession = kwmo.model.Session
    Workspace = kwmo.model.Workspace
    User = kwmo.model.User
    ChatMessage = kwmo.model.ChatMessage
    KfsNode = kwmo.model.KfsNode
    ChatRequest = kwmo.model.ChatRequest
    VncSession = kwmo.model.VncSession
    WSRequest = kwmo.model.WSRequest


# This class represents a workspace event.
class WSEvent:
    def __init__(self, kws_id, evt_id, major, minor, type, evt):
        self.kws_id = kws_id
        self.evt_id = evt_id
        self.major = major
        self.minor = minor
        self.type = type
        self.anp = ANP_msg().parse(evt)    

# This class represents a workspace fetcher.
class WSFetcher(object):
    # This function creates or loads a workspace fetcher object.
    def __init__(self, fetcher_path, workspace_id):
        self.ws_path = os.path.join(fetcher_path, str(workspace_id))

        # Make sure disk
        self.create_disk_skel()

        r = Workspace.get_by(id=workspace_id)
        if r:
            logger.info("Loading workspace %i." % ( workspace_id ) )
            self.ws = r

        else:
            logger.info("Creating workspace %i." % ( workspace_id ) )
            self.ws = Workspace(id=workspace_id, last_event_id=0, last_perm_check_id=0)

            logger.info("Creating root user id in workspace %i." % ( workspace_id ) )
            User.create_root_user(workspace_id)

            # Get information on the workspace that are not available in the event log.
            self.get_general_infos()

            # Create the root directory for share 0.
            self.create_root_directory(0)

    # This function creates the workspace skeleton to disk. If skeleton already exists, it does nothing.
    def create_disk_skel(self):

        # Create the workspace directories, if needed.
        if not os.path.exists(self.ws_path):
            os.mkdir(self.ws_path, 0777)
       
        # Create the wake-up file, if needed.
        if not os.path.exists(self.ws_path + "/wakeup"):
            tmp = open(self.ws_path + "/wakeup", "wb")
            tmp.close()
        
    # This function gets the general infos about the workspace which are not available with events.
    def get_general_infos(self):
        logger.debug("Fetching workspace %i additionnal infos from kcd db." % ( self.ws.id ) )
        results = exec_pg_select_rb(kcd_db_conn,
            "select creation_date, name, flags from kcd_kws_list where kws_id = %s" % ( ntos(self.ws.id) ) )
        row = results[0]
        self.ws.creation_date = row[0]
        self.ws.name = row[1]
        flags = row[2]
        self.ws.public = False
        if flags & KANP_KWS_FLAG_PUBLIC:
            self.ws.public = True
        if flags & KANP_KWS_FLAG_COMPAT_V2:
            self.ws.compat_v2 = True
            if self.ws.public: self.ws.secured = False
            else: self.ws.secured = True
        else:
            self.ws.compat_v2 = False
            self.ws.secured = ((flags & KANP_KWS_FLAG_SECURE ) > 0)

    # Check if workspace is deleted.
    def check_delete(self):
        # Get workspace deleted flag from the KCD database.
        logger.debug("Checking workspace %i deleted flag." % ( self.ws.id ) )
        results = exec_pg_select_rb(kcd_db_conn,
            "select flags from kcd_kws_list where kws_id = %s" % ( ntos(self.ws.id) ) )
        row = results[0]
        kcd_deleted = bool(row[0] & KANP_KWS_FLAG_DELETE)

        if kcd_deleted and not self.ws.deleted:
            # Workspace is deleted but local database doesn't know that yet.

            # Stop listening for this workspace.
            self.stop_listening()

            # Delete workspace.
            logger.debug("Deleting workspace %i." % ( self.ws.id ) )
            Workspace.delete_workspace(self.ws.id)

            # Commit change in sql alchemy session.
            SASession.commit()
            logger.info("Deleted workspace %i." % ( self.ws.id ) )

            # Delete workspace from the list.
            global kws_delete_list
            kws_delete_list += [self.ws.id]

    # This function creates the kfs root directory.
    def create_root_directory(self, share_id):
        logger.debug("Creating root directory for workspace %i, share %i." % ( self.ws.id, share_id ) )
        node = KfsNode()
        node.workspace_id = self.ws.id
        node.share_id = share_id
        node.inode_id = 0
        node.commit_id = 0
        node.user_id = 1
        node.name = ''
        node.cdate = 0
        node.mdate = 0
        node.inode_type = kfs_lib.KFS_DIR
        node.status = kfs_lib.KFS_STATUS_OK

    # This function starts listening for this workspace.
    def start_listening(self):
        logger.info("Listening for perm_checks for workspace %i." % ( self.ws.id ) )
        exec_pg_query_rb_on_except(kcd_db_conn, "LISTEN kws_%i_perm_check" % ( self.ws.id ) )

        logger.info("Listening for events for workspace %i." % ( self.ws.id ) )
        exec_pg_query_rb_on_except(kcd_db_conn, "LISTEN kws_%i_event_log" % ( self.ws.id ) )

        kcd_db_conn.commit()

        self.perm_check_flag = 1
        self.poll_flag = 1

    # This function stops listening for this workspace.
    def stop_listening(self):
        logger.info("Stopping listening for perm_checks for workspace %i." % ( self.ws.id ) )
        exec_pg_query_rb_on_except(kcd_db_conn, "UNLISTEN kws_%i_perm_check" % ( self.ws.id ) )

        logger.info("Stopping listening for events for workspace %i." % ( self.ws.id ) )
        exec_pg_query_rb_on_except(kcd_db_conn, "UNLISTEN kws_%i_event_log" % ( self.ws.id ) )

        kcd_db_conn.commit()

    # This function handle a perm_check notification.
    def handle_perm_check(self):
        # Update perm_check id in the workspace table.
        self.ws.last_perm_check_id += 1

        # Delete workspace if needed.
        self.check_delete()

        # Commit change in sql alchemy session.
        SASession.commit()
        logger.debug("Perm check commited to database.")

    # This function handles a received event.
    def handle_event(self, evt):
      
        evt_constant_name = str(get_kanp_constant(evt.type, "KANP_EVT_")) 
        logger.info("Received event: id=%i, type=%s (%s)." % ( evt.evt_id, str(evt_constant_name), str(evt.type) ) )

        # Dispatch the event.
        if evt.type == KANP_EVT_KWS_CREATED:
            logger.debug("Creating owner user in workspace %i." % ( self.ws.id ) )
            User.from_creation_evt(evt)
            self.ws.evt_user_id = max(evt.evt_id, self.ws.evt_user_id)
       
        elif evt.type == KANP_EVT_KWS_INVITED:
            users_list = User.from_invitation_evt(evt)
            self.ws.evt_user_id = max(evt.evt_id, self.ws.evt_user_id)
            logger.debug("Creating %i invited users in workspace %i." % ( len(users_list), self.ws.id ) )
            for user in users_list:
                logger.debug("Creating user %i in workspace %i." % ( user.id, self.ws.id ) )
        
        elif evt.type == KANP_EVT_KWS_USER_REGISTERED:
            user = User.update_registered_from_evt(evt)
            self.ws.evt_user_id = max(evt.evt_id, self.ws.evt_user_id)
            logger.debug("Updating registered user %i in workspace %i." % ( user.id, self.ws.id ) )

        elif evt.type == KANP_EVT_CHAT_MSG:
            ChatMessage.from_evt(evt)
            self.ws.evt_chat_id = max(evt.evt_id, self.ws.evt_chat_id)
            logger.debug("Creating chat message.")

        elif evt.type == KANP_EVT_KFS_PHASE_1:
            self.handle_evt_phase1(evt)
            self.ws.evt_kfs_id = max(evt.evt_id, self.ws.evt_kfs_id)

        elif evt.type == KANP_EVT_KFS_PHASE_2:
            self.handle_evt_phase2(evt)
            self.ws.evt_kfs_id = max(evt.evt_id, self.ws.evt_kfs_id)

        elif evt.type == KANP_EVT_VNC_START:
            vsess = VncSession.start_from_evt(evt)
            self.fill_vnc_data(vsess)
            self.ws.evt_vnc_id = max(evt.evt_id, self.ws.evt_vnc_id)
            logger.debug("Session %i started in workspace %i." % ( vsess.session_id, self.ws.id ) )
            
        elif evt.type == KANP_EVT_VNC_END:
            vsess = VncSession.stop_from_evt(evt)
            self.ws.evt_vnc_id = max(evt.evt_id, self.ws.evt_vnc_id)
            logger.debug("Session %i stopped in workspace %i." % ( vsess.session_id, self.ws.id ) )

        elif evt.type == KANP_EVT_PB_TRIGGER_CHAT:
            crequest = ChatRequest.from_evt(evt)
            self.ws.evt_skurl_id = max(evt.evt_id, self.ws.evt_skurl_id)
            logger.debug("Skurl chat request %i for user %i in workspace %i." % \
                ( crequest.request_id, crequest.user_id, self.ws.id ) )

        elif evt.type == KANP_EVT_PB_CHAT_ACCEPTED:
            crequest = ChatRequest.accept_from_evt(evt)
            self.ws.evt_skurl_id = max(evt.evt_id, self.ws.evt_skurl_id)
            logger.debug("Skurl chat request %i accepted for user %i in workspace %i." % \
                ( crequest.request_id, crequest.user_id, self.ws.id ) )

        elif evt.type == KANP_EVT_PB_TRIGGER_KWS:
            wsrequest = WSRequest.from_evt(evt)
            self.ws.evt_skurl_id = max(evt.evt_id, self.ws.evt_skurl_id)
            logger.debug("Skurl workspace creation request %i for user %i in workspace %i." % \
                ( wsrequest.request_id, wsrequest.user_id, self.ws.id ) )

        elif evt.type == KANP_EVT_KWS_PROP_CHANGE:
            self.handle_kws_prop_change(evt)
            self.ws.evt_ws_id = max(evt.evt_id, self.ws.evt_ws_id)
            self.ws.evt_user_id = max(evt.evt_id, self.ws.evt_user_id)

        else:
            logger.debug("Event %i not handled." % ( evt.evt_id ) )

        # Update the last event ID.
        self.ws.last_event_id = max(evt.evt_id, self.ws.last_event_id)
        
        # Commit change in sql alchemy session.
        SASession.commit()
        logger.debug("Event %i commited to database." % ( evt.evt_id ) )
        
        # Update the wake-up file.
        #tmp = open(self.ws_path + "/wakeup", "ab")
        #tmp.write("a")
        #tmp.close()
    
    def fill_vnc_data(self, vnc_session):
        results = exec_pg_select_rb(kcd_db_conn,
            "select port from kcd_kws_vnc_session \
                where kws_id=%s AND session_id = %s" % \
                ( ntos(vnc_session.workspace_id), ntos(vnc_session.session_id)))

        if len(results) > 0:
            row = results[0]
            port = row[0]
            vnc_session.port = port
        else:
            vnc_session.port = 9999 #invalid port


    
    def handle_evt_phase1(self, evt):
        kws_id = evt.anp.get_u64()
        date = evt.anp.get_u64()
        user_id = evt.anp.get_u32()
        share_id = evt.anp.get_u32()
        commit_id = evt.anp.get_u64()
        nb_changes = evt.anp.get_u32()
        logger.debug("Received phase 1 event with %i changes." % ( nb_changes ) )

        # Pass all files.
        for i in range(0, nb_changes):
            junk = evt.anp.get_u32()
            change_type = evt.anp.get_u32()

            logger.debug("Phase 1 loop... applying change %i, change_type: %i" % ( (i + 1), change_type ) )

            if change_type == KANP_KFS_OP_CREATE_FILE or change_type == KANP_KFS_OP_CREATE_DIR:
                t_inode_id = evt.anp.get_u64()
                t_parent_inode_id = evt.anp.get_u64()
                t_name = evt.anp.get_str()

                if change_type == KANP_KFS_OP_CREATE_FILE:
                    # Delete the matching deleted file, if any, so we can insert a new file in the table.
                    entry = KfsNode.get_by(workspace_id=kws_id, 
                                           share_id=share_id, 
                                           parent_inode_id=t_parent_inode_id,
                                           name=t_name, 
                                           status=kfs_lib.KFS_STATUS_DELETED)
                    if entry:
                        entry.delete()

                entry = KfsNode()
                entry.inode_id = t_inode_id
                entry.share_id = share_id
                entry.workspace_id = kws_id
                entry.user_id = user_id
                entry.parent = KfsNode.get_by(workspace_id=kws_id, share_id=share_id, inode_id=t_parent_inode_id)
                entry.commit_id = commit_id
                entry.name = t_name
                entry.cdate = date
                entry.mdate = date
                entry.file_size = None
                entry.file_hash = None
                
                if change_type == KANP_KFS_OP_CREATE_FILE:
                    entry.inode_type = kfs_lib.KFS_FILE
                    entry.status = kfs_lib.KFS_STATUS_PENDING
                    # File creation.
                    logger.debug(
                        "Creating file with status=pending: kws_id=%s, share_id=%s, inode_id=%s" % \
                        ( str(entry.workspace_id), str(entry.share_id), str(entry.inode_id) ) )

                elif change_type == KANP_KFS_OP_CREATE_DIR:
                    entry.inode_type = kfs_lib.KFS_DIR
                    entry.status = kfs_lib.KFS_STATUS_OK

                    # Directory creation.
                    logger.debug(
                        "Creating directory: kws_id=%s, share_id=%s, inode_id=%s" % \
                        ( str(entry.workspace_id), str(entry.share_id), str(entry.inode_id) ) )

            elif change_type == KANP_KFS_OP_UPDATE_FILE:
                # File update.
                inode_id = evt.anp.get_u64()

                # Update file.
                node = KfsNode.get_by(workspace_id=kws_id,
                                      share_id=share_id,
                                      inode_id=inode_id)
                node.user_id = user_id
                node.mdate = date
                node.commit_id = commit_id
                node.status = kfs_lib.KFS_STATUS_PENDING
                node.file_size = None
                node.file_hash = None

                logger.debug(
                    "Updating file: kws_id=%s, share_id=%s, inode_id=%s" % \
                    ( str(kws_id), str(share_id), str(inode_id) ) )

            elif change_type == KANP_KFS_OP_DELETE_FILE or change_type == KANP_KFS_OP_DELETE_DIR:
                inode_id = evt.anp.get_u64()

                # Note: Because of a hack in phase 2 handling where cancelled uploads are locally handled as
                # deleted files (although it's not the case in kcd), this must NOT check that
                # the file has a 0 status: it could actually have a 2 status.
                node = KfsNode.get_by(workspace_id=kws_id, share_id=share_id, inode_id=inode_id)

                if change_type == KANP_KFS_OP_DELETE_FILE:
                    # File deletion
                    logger.debug(
                        "Deleting file: kws_id=%s, share_id=%s, inode_id=%s" % \
                         ( str(kws_id), str(share_id), str(inode_id) ) )

                    node.status = kfs_lib.KFS_STATUS_DELETED

                else:
                    # Directory deletion
                    logger.debug(
                        "Deleting directory: kws_id=%s, share_id=%s, inode_id=%s" % \
                        ( str(kws_id), str(share_id), str(inode_id) ) )

                    node.deleteDeletedChilds()
                    node.delete()

            elif change_type == KANP_KFS_OP_MOVE_FILE or change_type == KANP_KFS_OP_MOVE_DIR:
                inode_id = evt.anp.get_u64()
                dest_parent_inode_id = evt.anp.get_u64()
                dest_parent_node = KfsNode.get_by(workspace_id=kws_id, share_id=share_id, inode_id=dest_parent_inode_id)
                dest_name = evt.anp.get_str()

                if change_type == KANP_KFS_OP_MOVE_FILE:
                    # File move.
                    logger.debug(
                        "Moving file: kws_id=%s, share_id=%s, inode_id=%s, dest_parent_inode_id=%s, dest_path=%s" % \
                        ( str(kws_id), str(share_id), str(inode_id), str(dest_parent_inode_id), dest_name ) )
                else:
                    # Directory move.                
                    logger.debug(
                         "Moving directory: kws_id=%s, share_id=%s, inode_id=%s, dest_parent_inode_id=%s, dest_path=%s" % \
                        ( str(kws_id), str(share_id), str(inode_id), str(dest_parent_inode_id), dest_name ) )

                node = KfsNode.get_by(workspace_id=kws_id, share_id=share_id, inode_id=inode_id)
                node.parent = dest_parent_node
                node.name = dest_name

            else:
                raise Exception("Phase 1 event: unknown change type: %i" % ( change_type ) )

    def handle_evt_phase2(self, evt):
        kws_id = evt.anp.get_u64()
        date = evt.anp.get_u64()
        user_id = evt.anp.get_u32()
        share_id = evt.anp.get_u32()
        commit_id = evt.anp.get_u64()
        nb_changes = evt.anp.get_u32()
        logger.debug("Received phase 2 event with %i changes." % ( nb_changes ) )

        for i in range(0, nb_changes):
            inode_id = evt.anp.get_u64()
            size = evt.anp.get_u64()
            hash = evt.anp.get_bin()

            kfsnode = KfsNode.get_by(workspace_id=kws_id, share_id=share_id, inode_id=inode_id)
            if kfsnode:
                kfsnode.user_id = user_id
                kfsnode.commit_id = commit_id
                kfsnode.mdate = date
                kfsnode.file_size = size
                kfsnode.file_hash = hash
                kfsnode.status = kfs_lib.KFS_STATUS_OK

                # File creation or update - final step
                logger.debug(
                    "Phase 2: updating info and status for file: kws_id=%s, share_id=%s, inode_id=%s" % \
                    ( str(kws_id), str(share_id), str(inode_id) ) )
            else:
                # File was _probably_ removed before being updated (another client). We can't be sure because 
                # files are not kept when deleted.
                logger.debug(
                    "Phase 2: ignoring file update because it does not exist (anymore?): kws_id=%s, share_id=%s, inode_id=%s" % \
                    ( str(kws_id), str(share_id), str(inode_id) ) )

        # Detect upload cancellations.
        kfsnodes = KfsNode.query.filter(and_(KfsNode.workspace_id == kws_id, KfsNode.share_id == share_id, \
                                             KfsNode.commit_id == commit_id, KfsNode.status == 0))
        if kfsnodes:
            for kfsnode in kfsnodes:
                logger.info(
                    "Phase 2: upload for file '%s' was cancelled: simulate deletion (kwmo only)." % ( kfsnode.name ) )
                kfsnode.user_id = user_id
                kfsnode.mdate = date
                kfsnode.file_size = 0
                kfsnode.file_hash = ''
                kfsnode.status = kfs_lib.KFS_STATUS_DELETED

    def handle_kws_prop_change(self, evt):
        kws_id = evt.anp.get_u64()
        date = evt.anp.get_u64()
        user_id = evt.anp.get_u32()
        nb_changes = evt.anp.get_u32()

        for i in range(0, nb_changes):
            property_type = evt.anp.get_u32()

            if property_type == KANP_PROP_KWS_NAME:
                kws_name = evt.anp.get_str()
                self.ws.name = kws_name

            elif property_type == KANP_PROP_KWS_FLAGS:
                kws_flags = evt.anp.get_u32()

                #CANCELLED# # Get the latest flags from KCD.
                #results = exec_pg_select_rb(kcd_db_conn,
                #    "SELECT flags FROM kcd_kws_list WHERE kws_id=%i" % \
                #        ( self.kws_id ) )
                #if not results or len(results) != 1:
                #    raise Exception("Received event assotiated to workspace ID %i, which does not exist in KCD." \
                #        % ( self.kws_id ) )
                #kws_flags = results[0]['flags']

                if kws_flags & KANP_KWS_FLAG_PUBLIC: self.ws.public = True
                else: self.ws.public = False

                if kws_flags & KANP_KWS_FLAG_FREEZE: self.ws.frozen = True
                else: self.ws.frozen = False

                if kws_flags & KANP_KWS_FLAG_DEEP_FREEZE: self.ws.deep_frozen = True
                else: self.ws.deep_frozen = False

                if kws_flags & KANP_KWS_FLAG_SECURE: self.ws.secured = True
                else: self.ws.secured = False

                if kws_flags & KANP_KWS_FLAG_COMPAT_V2: self.ws.compat_v2 = True
                else: self.ws.compat_v2 = False

            elif property_type == KANP_PROP_USER_NAME_ADMIN:
                user_id = evt.anp.get_u32()
                user_name = evt.anp.get_str()
                user = User.get_by(workspace_id=self.ws.id, id=user_id)
                user.admin_name = user_name

            elif property_type == KANP_PROP_USER_NAME_USER:
                ch_user_id = evt.anp.get_u32()
                user_name = evt.anp.get_str()
                user = User.get_by(workspace_id=self.ws.id, id=user_id)
                user.real_name = user_name

            elif property_type == KANP_PROP_USER_FLAGS:
                ch_user_id = evt.anp.get_u32()
                user_flags = evt.anp.get_u32()

                #CANCELLED# # Get the latest flags from KCD.
                #results = exec_pg_select_rb(kcd_db_conn,
                #    "SELECT flags FROM kcd_kws_users WHERE kws_id=%i AND user_id=%i" % \
                #        ( self.kws_id, ch_user_id ) )
                #if not results or len(results) != 1:
                #    raise Exception("Received event assotiated to workspace ID %i, user ID %i, which does not exist in KCD." \
                #        % ( self.kws_id, ch_user_id ) )
                #user_flags = results[0]['flags']

                user = User.get_by(workspace_id=self.ws.id, id=ch_user_id)
                if user_flags & KANP_USER_FLAG_ADMIN: user.admin = True
                else: user.admin = False
                if user_flags & KANP_USER_FLAG_MANAGER: user.manager = True
                else: user.manager = False
                if user_flags & KANP_USER_FLAG_LOCK: user.locked = True
                else: user.locked = False
                if user_flags & KANP_USER_FLAG_BAN: user.banned = True
                else: user.banned = False

            else:
                logger.debug(
                    "Property change ignored because it has an unknown type: kws_id=%s, user_id=%s, prop_type=%s." % \
                        ( str(kws_id), str(user_id), str(property_type) ) )

    # This function polls the workspace for new events.
    def poll(self):
        
        while 1:
            # Fetch the events by groups.
            limit = 100
        
            # Receive the events.
            results = exec_pg_select_rb(kcd_db_conn, 
                "select evt_id, major, minor, type, event \
                    from kcd_kws_event_log \
                    where kws_id=%s AND evt_id > %s order by evt_id limit %s" % \
                    ( ntos(self.ws.id), ntos(self.ws.last_event_id), ntos(limit)))
           
            if len(results) == 0: break
 
            for row in results:
                evt = WSEvent(self.ws.id, row[0], row[1], row[2], row[3], row[4])
                self.handle_event(evt)
            
# This function consumes all the notifications received from Postgres. If a
# notification is received, this function returns true.
def consume_pg_notif(db):
    global poll_kws_list_flag
    processed = 0
    
    while 1:
        notifs = db.notifies
        if not notifs or len(notifs) == 0:
            break
        processed = 1
        
        logger.debug("Found " + str(len(notifs)) + " notifications: " + str(notifs))
        
        # Notification for kws_list.
        while db.notifies:
            n = db.notifies.pop()
            if n.channel == "kws_list":
                poll_kws_list_flag = 1
        
            # Notification for a workspace.
            else:
                match = re.compile('kws_(\d+)_perm_check').match(n.channel)
                if match:
                    kws_id = int(match.group(1))
                    if kws_dict.has_key(kws_id):
                        kws_dict[kws_id].perm_check_flag = 1 
                else:
                    match = re.compile('kws_(\d+)_event_log').match(n.channel)
                    if match:
                        kws_id = int(match.group(1))
                        if kws_dict.has_key(kws_id):
                            kws_dict[kws_id].poll_flag = 1
                    else:
                        raise Exception("cannot match notification name " + n.channel)
    return processed

# This function waits for a notification from Postgres.
def wait_for_pg_notif(db):
    while 1:
        #db.conn.consumeInput()
        db.poll()
        if consume_pg_notif(db): return
        select.select([db], [], [], 0.1)
    
# This function starts listening to the workspace list.
def listen_kws_list():
    exec_pg_query_rb_on_except(kcd_db_conn, "LISTEN kws_list")
    kcd_db_conn.commit()

# This function returns the list of workspaces having an ID higher than the ID
# specified and that are not deleted.
def get_kws_id_list(last_id, max_kws_id):
    query =  "SELECT kws_id, bool(flags & %i) as deleted FROM kcd_kws_list WHERE kws_id > %s" % \
        (KANP_KWS_FLAG_DELETE, ntos(last_id))
    if max_kws_id != None: query += " AND kws_id <= %s" % ( ntos(max_kws_id) )
    results = exec_pg_select_rb(kcd_db_conn, query)
    s = []
      
    for row in results:
        kws_id = row[0]
        deleted = row[1]
        logger.info("Adding workspace %i to the list." % ( kws_id ) )
        s.append((kws_id, deleted))
    
    return s

# This function adds the workspaces received from the KAS to the workspace
# dictionary.
def handle_new_kws(kws_items):
    global kws_dict
    global last_kws_id
    
    for id, deleted in (kws_items):
        
        # Remember the highest workspace ID we fetched.
        last_kws_id = max(id, last_kws_id)
        
        if deleted:
            ws = Workspace.get_by(id=id)
            if ws and not ws.deleted:
                # Workspace is deleted in KCD but not locally. Delete it.
                logger.debug("Deleting local view of workspace %i." % ( id ))
                Workspace.delete_workspace(id)

                # Commit change in sql alchemy session.
                SASession.commit()
                logger.info("Deleted workspace %i." % ( id ) )

        else:
            # Add the workspace to the workspace dictionary.
            kws_dict[id] = kws = WSFetcher(fetcher_path, id)

            # Start listening to the workspace.
            kws.start_listening()

# This function polls the list of workspaces and handle new workspaces.
def poll_kws_list():
    handle_new_kws(get_kws_id_list(last_kws_id, max_kws_id))

# This function fetches new data about the workspaces and the workspace list.
def fetch_kws_data():
    global poll_kws_list_flag
    global kws_dict
    global kws_delete_list
   
    # Poll the workspace list.
    if poll_kws_list_flag:
        poll_kws_list_flag = 0
        logger.info("Polling kws list.")
        poll_kws_list()
        logger.debug("Polled kws list.")
    
    # Handle perm_checks.
    for id, kws in kws_dict.iteritems():
        if kws.perm_check_flag:
            kws.perm_check_flag = 0
            logger.debug("Handling perm_check in kws %i." % (id))
            kws.handle_perm_check()

    # Delete workspaces from list, if needed.
    for id in kws_delete_list:
        del kws_dict[id]
    kws_delete_list = []

    # Poll the workspaces.
    for id, kws in kws_dict.iteritems():
        if kws.poll_flag:
            kws.poll_flag = 0
            logger.info("Polling kws %i." % (id))
            kws.poll()

# This function contains the main loop of the fetcher.
def do_main_loop():

    # Do the same thing, over and over again. I wouldn't like to be this
    # program.
    while 1:
    
        # Fetch the new workspace data.
        logger.debug("Fetching workspaces data.")
        fetch_kws_data()

        # Wait for a notification.
        wait_for_pg_notif(kcd_db_conn)
        logger.debug("Got notification from database.")

# This function enables TCP keepalives.
def enable_keepalive():
    
    # We might be operating on a UNIX socket, with no way to tell with the
    # limited Python interface. Ignore failures to deal with that case.
    try:
        # The socket module insists to duplicate the file descriptor when it
        # creates a socket object from a numeric file descriptor. It does not
        # matter: both file descriptors refer to the same socket.
        sock = socket.fromfd(kcd_db_conn, socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_TCP, socket.TCP_KEEPIDLE, tcp_keepalive_time)
        sock.setsockopt(socket.SOL_TCP, socket.TCP_KEEPINTVL, tcp_keepalive_intvl)
        sock.setsockopt(socket.SOL_TCP, socket.TCP_KEEPCNT, tcp_keepalive_probes)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    except:
        pass
    
# This function starts the fetcher operations.
def start_fetcher(options):
    global kcd_db_conn
 
    # Connect to the kas postgres database on kcd host.
    logger.info("Connecting to the kcd database.")
    try:
        kcd_db_conn = get_kcd_db_conn(get_kcd_external_conf_object())
    except:
        logger.error("Could not connect to the kcd database.")
        raise
    logger.debug("Connected to the kcd database.")
    
    # Enable keepalives to detect failed connections before 2 hours pass.
    enable_keepalive()
    
    # Listen to the list of workspaces.
    logger.info("Listening to the list of workspaces.")
    listen_kws_list()
    
    # Enter the main loop.
    logger.info("Entering the main loop.")
    do_main_loop()

# Parse command line options.
def parse_cmd_line():
    global paster_config_path, min_kws_id, max_kws_id

    try:
        opts, args = getopt.getopt(sys.argv[1:], "s:", ["paster-config-file=","min-kws-id=", "max-kws-id="])
    except getopt.error, msg:
        sys.stderr.write("Could not parse parameters: " + str(msg) + "\n")
        sys.exit(2)
    for o, a in opts:
        if o in ("-s", "--paster-config-file"):
            paster_config_path = a
        elif o in ("--min-kws-id"):
            min_kws_id = int(a)
        elif o in ("--max-kws-id"):
            max_kws_id = int(a)

def main(globals, locals):
    global logger, last_kws_id

    # Get the options from command line.
    options = parse_cmd_line()
    if min_kws_id != None: last_kws_id = min_kws_id - 1 

    # Load modules an the global module scope.
    load_models_as_globals(paster_config_path, globals, locals)

    # Configure logging
    logging.config.fileConfig(paster_config_path)
    logger = logging.getLogger("kwsfetcher")

    # Log which config files were used.
    logger.debug("Using config files:")
    logger.debug("  paster:     " + str(paster_config_path) )

    try:

        logger.info("KWSFetcher starting...")

        # Start the fetcher.
        start_fetcher(options)
        
    except Exception, e:
        logger.error("Fatal exception:")
        for line in str(e).split("\n"):
            logger.error("Exception: " + line)
        for line in traceback.format_exc().split("\n"):
            logger.error("Traceback: " + line)
        logger.error("Exiting.")
        sys.exit(1)


main(globals(), locals())

