# from system
import os, cgi, logging, datetime
from sqlalchemy.sql import and_, asc
import simplejson

# from kpython
from kbase import PropStore
from kflags import Flags

# from local
from kwmo.lib import kwmolib
from kwmo.lib.perms import KWMOPermissions

import kfs_lib
from kwmo.model import Session
from kwmo.model import Workspace
from kwmo.model import User
from kwmo.model import KfsNode
from kwmo.model import ChatMessage
from kwmo.model import ChatRequest
from kwmo.model import VncSession
from kwmo.model import KfsUploadStatus
from kwmo.model.kcd import KcdPubEmailInfo

from kwmo.model.kfs_upload_status import UPLOAD_STATUS_PENDING, UPLOAD_STATUS_ERROR, UPLOAD_FAIL_GENERAL

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# This class represents a state request.
class StateRequest(Flags):

    STATE_FORCE_SYNC = 1<<0
    STATE_WANT_MEMBERS = 1<<1
    STATE_WANT_KFS = 1<<2
    STATE_WANT_CHAT = 1<<3
    STATE_WANT_VNC = 1<<4
    STATE_WANT_PERMS = 1<<6
    STATE_WANT_WS = 1<<7
    STATE_WANT_UPLOAD_STATUS = 1<<8
    STATE_WANT_PUBWS_INFO = 1<<9;
    
    # Derived constructor
    def __init__(self, flags, params):
        self._flags = flags
        self._params =  params

    # Returns parameters.
    def getParams(self):
        return self._params

    # Return parameter
    def getParam(self, key, def_val=None):
        try: return self._params[key]
        except Exception: return def_val

    # Return parameter
    def getStrParam(self, key, def_val=None):
        try: return str(self._params[key])
        except Exception: return def_val

    # Return parameter
    def getIntParam(self, key, def_val=None):
        try: return int(self._params[key])
        except Exception: return def_val

    # Dump object to str.
    def __str__(self):
        return '<StateRequest flags=%s params=%s>' % ( self.setFlagsToStr(), str(self._params) )

def state_request_get(c, session, flags, params, version=None, req_id=0):
    current_version = 1
    if not version: version = current_version

    d = PropStore()
    d.mode = session['mode']
    d.workspace = c.workspace
    d.is_admin = c.is_admin
    d.email_id = None
    if 'email_id' in session: d.email_id = session['email_id']
    d.perms = c.perms
    d.user = session['user'] ### can be null if not logged ###
    d.email_info = None
    d.identities = None
    if d.workspace.public and not d.is_admin:
        d.email_info = session['email_info']
        d.identities = session['identities']

    result = {}
    result["version"] = current_version
    result["req_id"] = req_id
    if version < current_version:
        # Version has changed... send no state.
        # Versions <1.9: will not reload but won't see any update either.
        # Versions >=1.9: will notice the version change and reload the page.
        # TODO if enough time: send bogus chat messages for <1.9?
        result["state"] = { }
    else:
        # Send the state.
        state_request_object = StateRequest(flags, params)
        result["state"] = state_request(d, state_request_object)

    return result

# This function handles the multiplexed ajax requests.
# NOTE:
# Synchonisation with client is done by sending a last event ID per module (chat, kfs, vnc, ...) with
# the updates, and by the client sending back that ID with every state request.
# This method is not perfect because:
#  - all data is not fetched atomically (race conditions)
#  - comparing the last event ID with the latest event of the workspace can cause 
#     unnecessary updates (changes in other (share IDs, chat channel IDs, directories) in the same workspace).
# See other notes below to know how this is handled per module, the issues, and what are the future limitations.
def state_request(d, sr):

    mode = d.mode
    workspace = d.workspace
    workspace_id = workspace.id
    user = d.user
    # Dictionary for storing results.
    res_dict = {}

    # Shortcut variables
    force_sync = sr.hasFlags(sr.STATE_FORCE_SYNC)
    last_perms_update_version = sr.getIntParam('last_perms_update_version', 0)
    last_evt_ws_id = sr.getIntParam('last_evt_ws_id', 0)
    last_evt_user_id = sr.getIntParam('last_evt_user_id', 0)
    last_evt_kfs_id = sr.getIntParam('last_evt_kfs_id', 0)
    last_evt_chat_id = sr.getIntParam('last_evt_chat_id', 0)
    last_evt_vnc_id = sr.getIntParam('last_evt_vnc_id', 0)
    last_evt_skurl_id = sr.getIntParam('last_evt_skurl_id', 0)
    kfs_share_id = 0
    chat_channel_id = sr.getIntParam('chat_channel_id', 0)

    #log.debug("state_request(): %s" % ( str(sr) ) )
    #log.debug("state_request(): fsync=%s user_evt_id=%i kfs_evt_id=%i chat_evt_id=%i vnc_evt_id=%i skurl_evt_id=%i" % \
    #    ( force_sync, last_evt_user_id, last_evt_kfs_id, last_evt_chat_id, last_evt_vnc_id, last_evt_skurl_id ) )

    # State variables
    moved = False
    iteration = 0

    #OUTSTANDING HTTP REQUEST## Open the wake-up file and go to the end.
    #OUTSTANDING HTTP REQUEST#wakeup_file_path = .........
    #OUTSTANDING HTTP REQUEST#wake_up_file = open(wakeup_file_path)
    #OUTSTANDING HTTP REQUEST#wake_up_file.seek(wakeup_size, os.SEEK_SET)

    #OUTSTANDING HTTP REQUEST#init_time = int(time.time())

    # Loop until we got what we needed.
    while 1:
        iteration += 1
        #log.debug("state_request(): iteration %i" % ( iteration ) )

        # Get the workspace from model.
        ws = Workspace.get_by(id=workspace_id)

        if sr.hasFlags(StateRequest.STATE_WANT_WS):
            if (ws.evt_ws_id > last_evt_ws_id) or force_sync:
                # Send the workspace informations.
                moved = True

                res_dict[StateRequest.STATE_WANT_WS] = {}
                data = ws_state_request(ws)
                res_dict[StateRequest.STATE_WANT_WS]['last_evt'] = ws.evt_ws_id
                res_dict[StateRequest.STATE_WANT_WS]['data'] = data

        if sr.hasFlags(StateRequest.STATE_WANT_MEMBERS):
            if (ws.evt_user_id > last_evt_user_id) or force_sync:
                if d.perms.hasPerm('users.list'):
                    # Send the members list.
                    moved = True

                    # NOTE (followup for the synchronisation note):
                    # All users are fetched and scanned to get the real
                    # last event ID, even in skurl mode.
                    # This avoids double updates with the same data, that would be possible
                    # if sending ws.evt_user_id instead (but note that this would not have been 
                    # a big problem in the users context because of the low probability of the race
                    # and the low cost of sending the same update twice).
                    res_dict[StateRequest.STATE_WANT_MEMBERS] = {}
                    t_last_evt_id, data = user_state_request(workspace_id, user, mode, ws.evt_user_id)
                    res_dict[StateRequest.STATE_WANT_MEMBERS]['last_evt'] = t_last_evt_id
                    res_dict[StateRequest.STATE_WANT_MEMBERS]['data'] = data

                else:
                    log.debug("No users list permission.")

        if sr.hasFlags(StateRequest.STATE_WANT_KFS):
            if (ws.evt_kfs_id > last_evt_kfs_id) or force_sync:
                if d.perms.hasPerm('kfs.list.share.%i' % (kfs_share_id)):
                    # Send the KFS status.
                    moved = True

                    if ws.public and not d.is_admin:
                        email_info = d.email_info
                        identities = d.identities
                        res_dict[StateRequest.STATE_WANT_KFS] = {}
                        data = pubws_kfs_state_request(ws, email_info, identities)
                        res_dict[StateRequest.STATE_WANT_KFS]['last_evt'] = ws.evt_kfs_id
                        res_dict[StateRequest.STATE_WANT_KFS]['data'] = data

                    else:
                        kfs_dir_dict = sr.getParam('kfs_dir')
                        kfs_dir = kfs_lib.WebKFSDirectory().from_dict(kfs_dir_dict)

                        assert kfs_dir.workspace_id == workspace_id
                        assert kfs_dir.share_id == kfs_share_id

                        # NOTE (followup for the synchronisation note):
                        # Not all nodes are fetched, and no locks are are used to wrap the multiple workspace and kfsnode tables
                        # accesses... this means that:
                        #  - ws.evt_kfs_id is not that reliable
                        #  - there are no quick and easy alternatives
                        # So, we use it anyways. With the current design, no changes updates are sent to the client: 
                        # all directory content is sent at every change, so problems are limited to 
                        # unnecessary updates:
                        # - when there are changes in other share_ids
                        # - when there are changes in other directories
                        # - when changes occur between fetching workspace and fetching kfs nodes 
                        res_dict[StateRequest.STATE_WANT_KFS] = {}
                        data = ws_kfs_state_request(kfs_dir)
                        res_dict[StateRequest.STATE_WANT_KFS]['last_evt'] = ws.evt_kfs_id
                        res_dict[StateRequest.STATE_WANT_KFS]['data'] = data
                else:
                    log.debug("No KFS list permission.")

        if sr.hasFlags(StateRequest.STATE_WANT_CHAT):
            if (ws.evt_chat_id > last_evt_chat_id) or force_sync:
                if d.perms.hasPerm('chat.list.channel.%i' % (chat_channel_id)):
                    # Send chat messages.
                    moved = True

                    # NOTE (followup for the synchronisation note):
                    # Changes are sent to client, so all fetched messages are scanned to ensure 
                    # the last event ID sent matches the messages sent.
                    # Issue: as not all messages from all channels are scanned,
                    # this could result in sending empty results all the time
                    # instead of not sending results (see the comparison with ws.evt_chat_id which can
                    # occur whenever there are messages in other chat channels which have bigger event IDs).
                    res_dict[StateRequest.STATE_WANT_CHAT] = {}
                    t_last_evt_id, data =  chat_state_request(workspace_id, chat_channel_id, last_evt_chat_id)
                    res_dict[StateRequest.STATE_WANT_CHAT]['last_evt'] = t_last_evt_id
                    res_dict[StateRequest.STATE_WANT_CHAT]['data'] = data
                else:
                    log.debug("No chat list permission. Rules: %s." % ( str(d.perms._rules) ) )

        if sr.hasFlags(StateRequest.STATE_WANT_VNC):
            if (ws.evt_vnc_id > last_evt_vnc_id) or force_sync:
                if d.perms.hasPerm('vnc.list'):
                    moved = True
                    res_dict[StateRequest.STATE_WANT_VNC] = {}
                    mode, vnc_list =  vnc_state_request(workspace_id, last_evt_vnc_id)
                    res_dict[StateRequest.STATE_WANT_VNC]['last_evt'] = ws.evt_vnc_id
                    res_dict[StateRequest.STATE_WANT_VNC]["list"] = vnc_list
                    res_dict[StateRequest.STATE_WANT_VNC]["mode"] = mode
                else:
                    log.debug("No shared applications list permission.")

        if sr.hasFlags(StateRequest.STATE_WANT_PERMS):
            #log.debug("Permissions rules: '%s'." % ( str(d.perms._rules) ) )
            if (d.perms.update_version > last_perms_update_version) or force_sync:
                # Send permissions (last, in case some other module updates the permissions).
                moved = True
                res_dict[StateRequest.STATE_WANT_PERMS] = perms_state_request(d.perms)

        if sr.hasFlags(StateRequest.STATE_WANT_UPLOAD_STATUS):
            # Support for multiple uploads is quite easy by the js code sending multiple client_random_id's (in an array),
            # and setting the status of each file in res_dict 
            client_random_id = sr.getIntParam('last_upload_random_id', 0)
            res_dict[StateRequest.STATE_WANT_UPLOAD_STATUS] = {}
            data = upload_status_state_request(client_random_id, workspace_id, kfs_share_id, user)
            res_dict[StateRequest.STATE_WANT_UPLOAD_STATUS]['data'] = data

        if sr.hasFlags(StateRequest.STATE_WANT_PUBWS_INFO):
            res_dict[StateRequest.STATE_WANT_PUBWS_INFO] = {}
            data = pubws_info_state_request(workspace_id, d.email_id)
            res_dict[StateRequest.STATE_WANT_PUBWS_INFO]['data'] = data


        # We're done.
        if moved:
            log.debug("state_request(): update at iteration %i." % (iteration) )
            break

        #OUTSTANDING HTTP REQUEST## TO CHECK BEFORE ENABLING!!!
        #OUTSTANDING HTTP REQUEST## Wait for something to happen.
        #OUTSTANDING HTTP REQUEST##if wait_for_event(req, init_time, wake_up_file):
        #OUTSTANDING HTTP REQUEST##    break

        break

    # cache updated workspace
    ###if moved:
    ###    sess.kws = kws

    #OUTSTANDING HTTP REQUEST## Close the wake-up file.
    #OUTSTANDING HTTP REQUEST#wake_up_file.close()

    #log.debug("handle_state(): res_dict='%s'" % ( str(res_dict) ) )

    return res_dict

# Get the permissions.
def perms_state_request(perms):
    return perms.to_dict()

# Get workspace informations.
def ws_state_request(ws):
    data = {}
    data['name'] = ws.name
    data['frozen'] = ws.frozen
    data['deep_frozen'] = ws.deep_frozen
    return data
    
def upload_status_state_request(client_random_id, workspace_id, kfs_share_id, user):
    data = {}
    tmp = {}
    if user:
        user_id = user['id']
        upload_status = KfsUploadStatus.get_by(client_random_id=client_random_id, workspace_id=workspace_id, share_id=kfs_share_id, user_id=user_id)
        if upload_status:
            tmp['status'] = upload_status.status
            tmp['failure_reason'] = upload_status.failure_reason
            if upload_status.status != UPLOAD_STATUS_PENDING:
                upload_status.remove_entry()
        else:
            tmp['status'] = UPLOAD_STATUS_PENDING
            tmp['failure_reason'] = -1
    else:
        tmp['status'] = UPLOAD_STATUS_ERROR
        tmp['failure_reason'] = UPLOAD_FAIL_GENERAL

    # TODO set uploaded bytes too.
    
    # Support for multiple file upload can be easily added
    data[client_random_id] = tmp

    return data

# Get pubws information.
def pubws_info_state_request(workspace_id, email_id):
    ei = KcdPubEmailInfo.get_by(kws_id=workspace_id, email_id=email_id)
    data = {}
    data['att_expire_flag'] = ei.att_expire_flag
    return data

# Get users list for the specified workspace.
# The list depends on weither user is logged in MODE_WS or in MODE_PUBWS mode.
def user_state_request(workspace_id, user, mode, last_evt_id):

    ulist = []

    # Get current user id (in pubws mode, it might not be defined yet.. just send WS creator in that case).
    cur_user_id = -1;
    if user: cur_user_id = user['id']

    # Get members.
    last_user_evt_id = 0
    for u in User.query.filter(User.workspace_id == workspace_id).order_by(asc(User.id)):

        # Send all users, except for pubws mode, where we just send only
        # the workspace creator and the current user (if defined).
        if mode == kwmolib.MODE_WS or \
            ( mode == kwmolib.MODE_PUBWS and \
                ( u.id in [0, 1, cur_user_id] ) ):
            ulist.append(u.to_dict())

        # Scan ALL the users fetched to get the latest event ID, no matter if in skurl mode or not.
        last_user_evt_id = u.evt_id

    log.debug("user_state_request(): Users list has %i entries." % ( len(ulist) ) )

    return max(last_evt_id, last_user_evt_id), ulist

def ws_kfs_state_request(kfs_dir):
    data = {}

    workspace_id = kfs_dir.workspace_id
    share_id = kfs_dir.share_id
    dir_inode_id = kfs_dir.inode_id

    # Get the current KFS path components.
    dir_node = KfsNode.get_by(workspace_id=workspace_id, share_id=share_id, inode_id=dir_inode_id)
    if not dir_node: raise Exception("Invalid KFS directory.")
    assert dir_node.isDir()
    components = dir_node.getPathComponents()
    data['components'] = components
   
    # Convert the components to dictionaries and add HTML-escaped names.
    client_components = []
    for c in components:
        d = c.to_dict()
        d['esc_name'] = cgi.escape(c.name)
        client_components.append(d)
    data['components'] = client_components
    #log.debug("kfs_state_request(): KFS Path: '%s'." % ( "/".join(map(lambda x: x.name, client_components)) ) )

    # Get sub-directories.
    nodes = dir_node.getSubdirs()
    # Convert nodes list.
    l = []
    for node in nodes:
        # FIXME: should use a custom object which would initialize from nodes and contain only needed infos.
        d = node.to_dict()
        d['esc_name'] = cgi.escape(node.name)
        d['files_count'] = node.getOKFilesCount()
        d['subdirs_count'] = node.getSubdirsCount()
        del d['file_hash'] # cannot be jsonified - not used anyways
        l.append(d)
    data['dirs'] = l
    log.debug("kfs_state_request(): Directory %s has %i sub-directories." % ( str(kfs_dir), len(l) ) )

    # Get files inside the current directory.
    nodes = dir_node.getAllButDeletedFiles()
    
    # Convert nodes list.
    l = []
    for node in nodes:
        # FIXME: should use a custom object which would initialize from nodes and contain only needed infos.
        d = node.to_dict()
        d['esc_name'] = cgi.escape(node.name)
        del d['file_hash'] # cannot be jsonified - not used anyways
        l.append(d)
    data['files'] = l
    log.debug("kfs_state_request(): Directory %s has %i files." % ( str(kfs_dir), len(l) ) )
   
    return data

def pubws_get_identity_files(workspace, email_info, identity, share_id, sender=False):
    workspace_id = workspace.id
    name = identity['email']
    node = KfsNode.get_by(workspace_id=workspace_id, share_id=share_id, parent_inode_id=0, inode_type=kfs_lib.KFS_DIR, 
        name=name)
    if node:
        #log.debug("Directory '%s' found: inode_id=%i." % ( name, node.inode_id ) )
        name = kfs_lib.get_kfs_skurl_subject(email_info['date'], email_info['subject'])
        node2 = KfsNode.get_by(workspace_id=workspace_id, share_id=share_id, parent_inode_id=node.inode_id, 
            inode_type=kfs_lib.KFS_DIR, name=name)
        if node2:
            #log.debug("Directory '%s' found: inode_id=%i." % ( name, node2.inode_id ) )
            if sender:
                name = "Original attachments"
                node3 = KfsNode.get_by(workspace_id=workspace_id, share_id=share_id, parent_inode_id=node2.inode_id, 
                    inode_type=kfs_lib.KFS_DIR, name=name)
                if node3:
                    #log.debug("Directory 3 '%s' existing." % ( name ) )
                    return node3.getFiles()
                else:
                    #log.debug("Directory 3 '%s' not found." % ( name ) )
                    pass
            else:
                #log.debug("Directory 2 '%s' existing." % ( name ) )
                return node2.getFiles()
        else:
            #log.debug("Directory 2 '%s' not found." % ( name ) )
            pass
    else:
        #log.debug("Directory '%s' not found." % ( name ) )
        pass
    return None

def pubws_kfs_state_request(ws, email_info, identities):
    data = {}

    share_id = 0

    # For sender and recipients, get the files in their associated directory, if present.
    user_files = data['user_files'] = {}
    
    i = -1
    for identity in [identities[0]] + identities:
        i += 1
        sender = (i == 0)
        nodes = pubws_get_identity_files(ws, email_info, identity, share_id, sender=sender)
        l = []
        if nodes:
            dir_present = 1
            for node in nodes:
                d = node.to_dict()
                d['esc_name'] = cgi.escape(node.name)
                del d['file_hash'] # cannot be jsonified - not used anyways
                l.append(d)
        else:
            dir_present = 0
        if sender:
            user_files['sender'] = l
            user_files['sender_dir_present'] = dir_present
        else:
            user_files[identity['email']] = l
    return data

def chat_state_request(workspace_id, channel_id, last_evt_id):
    data = {}

    if last_evt_id == 0:
        # Send all messages.
        log.debug("Chat messages request: sending all messages.")
        data["mode"] = "all"
        messages = ChatMessage.query.filter(and_(ChatMessage.workspace_id == workspace_id,
            ChatMessage.channel_id == channel_id)).order_by(asc(ChatMessage.date))
    else:
        # Sending an update only.
        log.debug("Chat messages request: sending newest messages.")
        data["mode"] = "update"
        messages = ChatMessage.query.filter(and_(ChatMessage.workspace_id == workspace_id,
            ChatMessage.channel_id == channel_id,
            ChatMessage.evt_id > last_evt_id))

    l = []
    for message in messages:
        d = message.to_dict()
        d['msg'] = cgi.escape(d['msg'])
        l.append(d)
        last_evt_id = max(last_evt_id, d['evt_id'])
    data['messages'] = l

    #log.debug("Chat messages request: data='%s'" % ( str(data) ) )
    log.debug("Chat messages request: data has %s messages." % ( str(len(data["messages"])) ) )

    return last_evt_id, data


def vnc_state_request(workspace_id, last_evt_id):    
    vnc_list = []
    mode = "all" #For now simply send all the vnc list. If needed, "upate mode" might be added later on, sending a list containg changes only(new and deleted sessions).
    result = VncSession.query.filter(VncSession.workspace_id == workspace_id)        
        
    for vnc_session in result:
        w = vnc_session.to_dict()
        vnc_list.append(w)
        #last_evt_id = max(last_evt_id, w['session_id'])

    return mode, vnc_list


