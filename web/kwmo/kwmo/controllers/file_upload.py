import logging
import simplejson, time

from cgi import FieldStorage
from pylons import request, response, session, config, tmpl_context as c
from pylons.controllers.util import abort, redirect_to

from kwmo.lib.base import BaseController, render
from kwmo.model.kcd.kcd_kws_kfs_current_view import KcdKwsKfsCurrentView
from kwmo.model.kfs_upload_status import KfsUploadStatus
from kfs_lib import *
from kwmo.lib.file_upload import KFSWSGIFilesUpload
from kwmo.lib.config import get_cached_kcd_external_conf_object
from kwmo.lib.kwmo_kcd_client import KcdClient
from sqlalchemy.sql import and_, or_

log = logging.getLogger(__name__)

class FileUploadController(BaseController):
    requires_auth = ['upload']

    # This action uploads file(s) to KCD asynchronously (while being uploaded).
    # It relies on the fact that nobody read environ['wsgi.input'] yet.
    # Since it is read when accessing POSTED vars, which is done in the Routing middleware, we surround it with 
    # DeferPOSTParsing() and UnDeferPOSTParsing() midddlewares, which temporarily hides and replaces some environment
    # variable so the application thinks there is an empty body.

    def upload(self, workspace_id):
        try:
            self.upload_status = None
            result = self._upload(workspace_id)

            if self.upload_status is not None:
                self.upload_status.update_status_success()

            return result
 
        except Exception, e:
            if self.upload_status is not None:
                self.upload_status.update_status_faliure(e)
        
            if config['debug']:
                # Bypass the TeamboxDebugMiddleware, which sends the exception to the html body,
                # because when an upload is interupted without stdin being parsed, no data gets to the browser.
                # Send exception to log instead.
                import sys, traceback

                exceptionType, exceptionValue, exceptionTraceback = sys.exc_info()

                # Log exception and debugging informations.
                log.error("Upload exception:")
                for t in traceback.format_tb(exceptionTraceback):
                    log.debug("%-9s: '%s'" % ( "trace", str(t) ) )
                log.error("%-9s: '%s'" % ( "exception", str(exceptionType) + str(e) ) )

                # Return empty body (it's ignored anyways).
                return ''

            else:
                raise

    def _upload(self, workspace_id):
        workspace_id = int(workspace_id)
        
        # Shortcuts
        share_id = 0
        user_id = session['user']['id']
        user_email = session['user']['email']
        client_random_id = None

        # Permissions verification
        log.debug(str(c.perms.to_dict()))
        if not c.perms.hasPerm('kfs.upload.share.%i' % (share_id)):
            log.error("Upload denied: user has not the right permissions.")
            return abort(403)

        email_id = 0

        parent_inode_id = None
        parent_commit_id = None
        filename = None
        inode_id = None
        commit_id = None
        if c.workspace.public:
            email_id = session['email_id']
            email_date = session['email_info']['date']
            email_subject = session['email_info']['subject']

            kc = KcdClient(get_cached_kcd_external_conf_object())
            kcd_dir = kc.kcd_kfs_create_skurl_directories(workspace_id, share_id, user_id, 
                user_email, email_id, email_date, email_subject)
            parent_inode_id = kcd_dir.inode
            parent_commit_id = kcd_dir.commit_id
            
        else:
            inode_id = request.GET.get('inode_id', None)

            client_random_id = request.GET['client_random_id']
            log.debug("about to upload a file with client_random_id %i" % (int(client_random_id)))
 
            if inode_id:
                # Updating a file.
                inode_id = long(inode_id)
                node = KcdKwsKfsCurrentView.get_by(kws_id=workspace_id, share_id=share_id, inode=inode_id)
                if not node: raise Exception("File does not exist.");
                filename = node.entry_name
                commit_id = node.commit_id

            else:
                # Get current kfs_dir object from request.
                # (It must be sent in the url AND NOT in the posted data for asynchroneous upload to work.)
                kfs_dir_json = request.GET['kfs_dir']
                kfs_dir_dict =  simplejson.loads(kfs_dir_json)
                assert kfs_dir_dict['workspace_id'] == workspace_id
                assert kfs_dir_dict['share_id'] == share_id

                # Get the dir node.
                kcd_dir = KcdKwsKfsCurrentView.get_by(kws_id=workspace_id, share_id=share_id, inode=kfs_dir_dict['inode_id'])
                parent_inode_id = kcd_dir.inode
                parent_commit_id = kcd_dir.commit_id

        # Helper function what will be used later in the asynchronous upload.
        # Get node by name:
        kfs_get_node_by_name = lambda x: KcdKwsKfsCurrentView.query.filter(
                                            and_(KcdKwsKfsCurrentView.kws_id == workspace_id,
                                                 KcdKwsKfsCurrentView.share_id == share_id,
                                                 KcdKwsKfsCurrentView.parent_inode == parent_inode_id,
                                                 KcdKwsKfsCurrentView.entry_name == x)).first()
        # Set upload failure:
        set_upload_failure = None
       
        if client_random_id is not None:
            self.upload_status = KfsUploadStatus.create_save(workspace_id, share_id, user_id, client_random_id)
            set_upload_failure = lambda e: self.upload_status.update_status_faliure(e)

            #TODO: lamda function that should be used to update the progress of the uploaded file(bytes written+status)
            #kfs_update_progess_status = lambda num_bytes_written, status: self.upload_status.update(num_bytes_written, status)
            
        # Asynchronously upload files to KCD.
        kcd_conf = get_cached_kcd_external_conf_object()

        if c.workspace.public:
            # Virtual skurl file upload limit is 50MB but the only available size is the POST size.
            # POSTs up to 60MB are allowed.
            if 'CONTENT_LENGTH' in request.environ and int(request.environ['CONTENT_LENGTH']) > 60 * 1024 * 1024:
                log.debug("File upload too big: %i bytes." % ( int(request.environ['CONTENT_LENGTH']) ) )
                # Read wsgi.input, otherwise it doesn't work.
                # FIXME: stop connection here and warn user (using Ajax?)
                #        Can we render (reliably) before stdin is read? Seems not, to re-test.
                FieldStorage(fp=request.environ['wsgi.input'], environ=request.environ)
                return simplejson.dumps({ "result" : "error", "error" : "File is too big: limit is 50MB." })
        fu = KFSWSGIFilesUpload(kcd_conf, request.environ, workspace_id, email_id, share_id, user_id, 
            parent_inode_id, parent_commit_id, filename, inode_id, commit_id, kfs_get_node_by_name,
            set_upload_failure)
        fs = fu.run()
        
        return simplejson.dumps({ "result" : "ok" })

