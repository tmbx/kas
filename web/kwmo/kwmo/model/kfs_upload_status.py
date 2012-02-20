from kwmo.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *

import datetime
# from local project
from kwmo.model.user import User
from kfs_lib import KFS_DIR, KFS_FILE, KFS_STATUS_OK, KFS_STATUS_DELETED
from kwmo.lib.file_upload import BadExtentionException
from sqlalchemy.databases.postgres import PGBigInteger
from kwmo.model import Session as SASession
import logging

import kanp

log = logging.getLogger(__name__)

# Possible values of the status field
UPLOAD_STATUS_SUCCESS = 0
UPLOAD_STATUS_ERROR = 1
UPLOAD_STATUS_PENDING = 2

# Possible upload faliure reasons.
UPLOAD_FAIL_GENERAL = 1
UPLOAD_FAIL_LICENSE = 2
UPLOAD_FAIL_ENCODING = 3
UPLOAD_FAIL_FILE_EXTENSION = 4

log = logging.getLogger(__name__)

# Ideally this model should be stored in some cache (non persistent) shared store.
class KfsUploadStatus(Entity):
    using_options(shortnames=True)
    
    # Composite primary key
    client_random_id = Field(Integer, primary_key=True, autoincrement=False)
    share_id = Field(Integer, primary_key=True, autoincrement=False)
    user_id = Field(Integer, primary_key=True, autoincrement=False)
    workspace_id = Field(PGBigInteger, primary_key=True, autoincrement=False)

    status = Field(Integer)
    failure_reason = Field(Integer)
    created = Field(TIMESTAMP, default=datetime.datetime.now())
    
    # Fields not in use right now
    uploaded_size = Field(PGBigInteger)
    name = Field(Text)
    
    @staticmethod
    def create_save(workspace_id, share_id, user_id, client_random_id):
        instance = KfsUploadStatus()
        instance.workspace_id = workspace_id
        instance.share_id = share_id
        instance.user_id = user_id
        instance.client_random_id = client_random_id
        instance.status = UPLOAD_STATUS_PENDING
        SASession.commit()
        return instance
    
    def update_status_success(self):
        self.status = UPLOAD_STATUS_SUCCESS
        SASession.commit()
    
    def update_status_faliure(self, e):
        self.status = UPLOAD_STATUS_ERROR
        if isinstance(e, kanp.KANPFailure):
            if k.errno == kanp.KANP_RES_FAIL_RESOURCE_QUOTA: self.failure_reason = UPLOAD_FAIL_LICENSE
            else: self.failure_reason = UPLOAD_FAIL_GENERAL
        elif isinstance(e, BadExtentionException):
            self.failure_reason = UPLOAD_FAIL_FILE_EXTENSION
        elif isinstance(e, UnicodeEncodeError):
            self.failure_reason = UPLOAD_FAIL_ENCODING
        else:
            self.failure_reason = UPLOAD_FAIL_GENERAL
        SASession.commit()
    
    # Remove status from db
    def remove_entry(self):
        self.delete()
        SASession.commit()
        
    #def update(num_bytes_written, status)
    #    self.uploaded_size = self.uploaded_size + num_bytes_written
    #    self.status = status
    #    SASession.commit()

