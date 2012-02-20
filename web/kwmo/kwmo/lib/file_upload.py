from kfs_lib import *
from kwmo.lib.kwmo_kcd_client import KcdClient
from kcgi import FieldStorage

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# Get file extension.
def get_file_ext(file_name):
    arr = file_name.split('.')
    if len(arr) > 1: return arr[-1]
    else: return None

# Exception: file has not the right extension.
class BadExtentionException(Exception):
    pass

# Asynchronous KFS file uploads
class KFSWSGIFilesUpload:
    def __init__(self, kcd_conf, environ, workspace_id, email_id, share_id, user_id, 
        parent_inode_id, parent_commit_id, filename, inode_id, commit_id, kfs_get_node_by_name, set_upload_failure):

        log.debug("KFSWSGIFilesUpload() instantiated: fp=%s" % ( environ['wsgi.input'] ) )
        self.kcd_conf = kcd_conf
        self.environ = environ
        self.user_id = user_id
        self.workspace_id = workspace_id
        self.email_id = email_id
        self.share_id = share_id
        self.parent_inode_id = parent_inode_id
        self.parent_commit_id = parent_commit_id
        self.filename = filename
        self.inode_id = inode_id
        self.commit_id = commit_id
        self.kfs_get_node_by_name = kfs_get_node_by_name
        self.set_upload_failure = set_upload_failure
        self.current_file = None
        self.buffer = None
        self.buffer_size = None

    def run(self):
        log.debug("KFSWSGIFilesUpload().run() called")

        try:
            fs = FieldStorage(fp=self.environ['wsgi.input'], 
                environ=self.environ, kfs_upload=self)
        finally:
            try:
                self.kcd_client.close()
            except Exception, e:
                log.debug("Got exception while closing KCD client: '%s'." % ( str(e) ) )

        return fs

    def kfs_make_file(self, field_storage):
        log.debug("kfs_make_file() called")

        # Get the file name as sent by the browser.
        upload_filename = field_storage.filename
        upload_filename = upload_filename[max(upload_filename.rfind("/"),upload_filename.rfind("\\"))+1:]

        # Convert to unicode string.
        upload_filename = unicode(upload_filename, encoding='utf8')

        if self.inode_id:
            if get_file_ext(self.filename) != get_file_ext(upload_filename):
                # The file sent does not have the same file exception as the 
                # file that must be updated.
                raise BadExtentionException()
            filename = self.filename
            
        else:
            # Strip everything before and including  the last '/' or '\' if any, in the file name.
            filename = upload_filename

            if len(filename) < 1: raise ErrorMsg("File name is empty. Provided filename was '%s'." % ( field_storage.filename ) )

        # Initialize file upload to KCD.
        log.debug("kfs_make_file(): field storage: '%s', filename: '%s'" % ( str(field_storage), filename) )
        self.init_upload(filename)

        return self

    def init_upload(self, filename):
        log.debug("init_upload(): filename=%s" % ( filename ) )

        self.current_file = True
        self.buffer = ''
        self.buffer_size = 0
        self.hash = hashlib.md5()

        # Instantiate a KCD client.
        self.kcd_client = KcdClient(self.kcd_conf)

        # Generate a list of files to upload with all the needed details.
        ufiles = []

        uf = KFSUploadFile()
        uf.workspace_id = self.workspace_id
        uf.share_id = self.share_id
        uf.user_id = self.user_id
        uf.name = filename
        uf.parent_inode_id = self.parent_inode_id
        uf.parent_commit_id = self.parent_commit_id

        if self.inode_id:
            # Update file.
            uf.kfs_op = kanp.KANP_KFS_OP_UPDATE_FILE
            uf.inode = self.inode_id
            uf.commit_id = self.commit_id

            log.debug("Updating file: %s" % ( str(uf.to_dict()) ) )

        else:
            # Detect if this creates or updates file.
            uf.parent_inode_id = self.parent_inode_id
            uf.parent_commit_id = self.parent_commit_id

            log.debug("Checking if file exist: workspace_id='%s', share_id='%s', dir_inode='%s', name='%s'" % \
                ( str(uf.workspace_id), str(uf.share_id), str(uf.parent_inode_id), filename ) )
            entry = self.kfs_get_node_by_name(filename)
            if entry:
                log.debug("This file already exists... updating it.")
                uf.kfs_op = kanp.KANP_KFS_OP_UPDATE_FILE
                uf.inode = entry.inode
                uf.commit_id = entry.commit_id
            else:
                log.debug("This file does not exists... creating it.")
                uf.kfs_op = kanp.KANP_KFS_OP_CREATE_FILE

        ufiles.append(uf)

        # Get upload ticket.
        ticket = self.kcd_client.get_kcd_upload_ticket(self.workspace_id, self.share_id, self.user_id)
        log.debug("Got an upload ticket that contains '%i' bytes." % ( len(ticket) ))

        # Connect to KCD.
        self.kcd_client.connect()

        # Select KFS role.
        self.kcd_client.select_role(kanp.KANP_KCD_ROLE_FILE_XFER)
        log.debug("kfs_create_dir(): selected kfs role.")

        # Create the directory.
        self.kfso = KFSOperations(ufiles, self.kcd_client, self.kcd_client)
        log.debug("AFTER KFSO")
        self.kfso.phase_one(self.email_id, ticket)
        log.debug("AFTER PHASE1")

        errors = 0
        for f in ufiles:
            if f.kfs_error:
                log.error("File '%s' could not be uploaded because '%s'." % ( str(f.name), str(f.kfs_error) ) )
                if self.set_upload_failure:
                    self.set_upload_failure("File '%s' could not be uploaded because '%s'." % \
                                            ( str(f.name), str(f.kfs_error) ) )
                errors += 1
        if errors == len(ufiles):
            raise Exception("All files could not be uploaded.")

    def seek(self, i=None):
        log.debug("seek(): no-op")

    def read(self, i=None):
        log.debug("read(): no-op")
        return None

    def write(self, data):
        data_len = len(data)

        #log.debug("write(): size=%i" % ( data_len ) )

        # Update hash
        self.hash.update(data)

        # Add data to buffer
        self.buffer += data
        self.buffer_size += data_len

        if self.buffer_size >= 200 * 1024: # 200K
            # Send data chunk.
            self.send_chunk()

    def finish_file(self):
        # Send latest chunks, if needed.
        while self.buffer:
            self.send_chunk()

        # Commit last file.
        log.debug("finish_file(): committing file.")
        self.commit_file()

    def send_chunk(self):
        # Get chunk from buffer.
        chunk = self.buffer[0:KFS_CHUNK_SIZE]
        chunk_len = len(chunk)
        self.buffer = self.buffer[chunk_len:]
        self.buffer_size -= chunk_len

        # Create a chunk sub-message.
        chunk_subm = self.kfso.phase_2_create_chunk_submessage(chunk)

        # Send the chunk.
        self.kfso.phase_2_send_message_with_one_submessage(chunk_subm)

    def commit_file(self):
        hash = self.hash.digest()

        log.debug("commit_file(): hash='%s'" % ( hash.encode('hex') ) )

        # Create a submessage.
        commit_subm = self.kfso.phase_2_create_commit_submessage(hash)

        # Send the submessage as the only submessage in a message.
        self.kfso.phase_2_send_message_with_one_submessage(commit_subm)

        self.current_file = None
        self.buffer = None
        self.buffer_size = None

