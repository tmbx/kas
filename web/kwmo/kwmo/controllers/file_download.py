import logging
import simplejson
import time

from pylons import request, response, session,config, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kfs_lib import *
from kwmo.lib.file_download import kfs_download_generator
from kwmo.model.kfs_node import KfsNode
from kwmo.lib.kwmo_kcd_client import KcdClient
from kwmo.lib.config import get_cached_kcd_external_conf_object

log = logging.getLogger(__name__)

class FileDownloadController(BaseController):
    
    requires_auth = ['download']

    # Send download notifications.
    send_notification = True

    # Download a file.
    def download(self, workspace_id):
        workspace_id = int(workspace_id)

        # Shortcuts
        share_id = 0
        user_id = session['user']['id']
        kcd_conf = get_cached_kcd_external_conf_object()

        # Permissions verification
        if not c.perms.hasPerm('kfs.download.share.%i' % (share_id)):
            log.error("File download denied: user has not the right permissions.")
            # FIXME download permission error: get rid of 403 error: send an errror file?
            return abort(403)
 
        # Get kfs_file object from request.
        web_kfs_file_json = request.params.get('kfs_file')
        web_kfs_file_dict =  simplejson.loads(web_kfs_file_json)
        web_kfs_file = WebKFSFile().from_dict(web_kfs_file_dict)
        assert web_kfs_file.workspace_id == workspace_id
        assert web_kfs_file.share_id == share_id

        # Get the kfs node associted to it.
        kfs_node = KfsNode.get_by(workspace_id=workspace_id, 
                                    share_id=web_kfs_file.share_id,
                                    inode_id=web_kfs_file.inode_id)

        if c.workspace.public and not c.is_admin:
            # Check that the user has rights to download from this path.
            kfs_dir = kfs_node.parent
            if not kfs_dir:
                raise Exception("Public workspace file download: bad directory(0).") 
            if kfs_dir.name == "Original attachments":
                kfs_dir = kfs_dir.parent
            expected_dir_name = get_kfs_skurl_subject(session['email_info']['date'], session['email_info']['subject'])
            if kfs_dir.name != expected_dir_name:
                raise Exception("Public workspace file download: bad directory(1).")
            kfs_dir_parent = kfs_dir.parent
            if not kfs_dir_parent: 
                raise Exception("Public workspace file download: bad directory(2).")
            if kfs_dir_parent.parent_inode_id != KFS_ROOT_INODE_ID: 
                raise Exception("Public workspace file download: bad directory(3).");
            identities_emails = map(lambda x: x['email'], session['identities'])
            if kfs_dir_parent.name not in identities_emails:
                raise Exception("Public workspace file download: bad directory(4).");

        # Get download mode.
        mode = request.params.get('mode', 'save')
        ctype = None

        if mode == 'open':
            # Guest mime type
            import mimetypes
            ctype, cencoding = mimetypes.guess_type(kfs_node.name, False) # not strict
            #ctype, cencoding = mimetypes.guess_type(kfs_node.name, True) # strict

        if not ctype:
            # Download mime type
            ctype, cencoding = ('application/octet-stream', None)
 
        kfs_files = [kfs_node.to_dict()]

        # Set the content type and the headers.
        response.headers['Content-Type'] = ctype
        #if cencoding:
        #    response.headers['Content-Encoding'] = cencoding
        response.headers['Content-disposition'] = str('attachment; filename="%s"' % ( kfs_node.name.encode('latin1') ))
        response.headers['Content-Transfer-Encoding'] = 'binary'

        # Use a custom header that will be replaced in a middleware (workaround for
        # the Content-Length header being dropped somewhere in pylons).
        response.headers['X-Content-Length'] = str(kfs_node.file_size)

        # These headers are necessary for the download to work on IE.
        response.headers['Cache-Control'] = 'maxage=3600'
        response.headers['Pragma'] = 'public'

        if self.send_notification:
            # Send download notification to KCD.
            pubws_email_id = 0
            if c.workspace.public:
                pubws_email_id = session['email_id']
            kc = KcdClient(kcd_conf)

            try:
                kc.send_download_notification(workspace_id, user_id, kfs_node.share_id, kfs_node.inode_id, 
                    kfs_node.commit_id, pubws_email_id)
                log.debug(("Sent download notification: workspace_id=%i, user_id=%i, share_id=%i" + \
                    ", inode_id=%i, commit_id=%i, pubws_email_id=%i.") % \
                    ( workspace_id, user_id, kfs_node.share_id, kfs_node.inode_id, kfs_node.commit_id, 
                      pubws_email_id ) )
            except Exception, e:
                log.error("Sending download notification failed: '%s'." % ( str(e) ) )
        else:
            log.debug("Not sending download notification: user is admin: workspace_id=%i, user_id=%i." % \
                ( workspace_id, user_id ) ) 

        return kfs_download_generator(kcd_conf, kfs_node.workspace_id, kfs_node.share_id, user_id, kfs_files)

