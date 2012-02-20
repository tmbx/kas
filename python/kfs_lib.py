import os, ConfigParser, hashlib, stat, struct, logging

# from kpython
import kbase
from kpg import *
from StringIO import StringIO

# local
import kanp
import kcd_client
from kcdpg import KCD_KWS_LOGIN_TYPE_KWMO

# KFS Constants.
KFS_CHUNK_SIZE = 256 * 1024
KFS_FILE = 1
KFS_DIR = 2
KFS_NODE_TYPES = [KFS_FILE, KFS_DIR]
KFS_STATUS_PENDING = 0
KFS_STATUS_OK = 1
KFS_STATUS_DELETED = 2
KFS_STATUSES = [KFS_STATUS_PENDING, KFS_STATUS_OK, KFS_STATUS_DELETED]
KFS_ROOT_INODE_ID = 0
KFS_ROOT_COMMIT_ID = 0

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# Replace bad characters in a skurl email subject for directory creation.
def get_kfs_skurl_escaped_subject(s, replacement_char='_'):
    allowed_chars = [
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
                    ]
    new_s = ''
    for c in s:
        if allowed_chars[ord(c)] == 1:
            new_s += c
        else:
            new_s += replacement_char
    return new_s

# Convert a skurl email subject into a valid KFS directory.
def get_kfs_skurl_subject(date, subject):
    d = time.strftime('%Y-%m-%d %Hh%Mm%S', time.gmtime(date))
    if subject == '':
        s = 'No subject'
    else:
        s = get_kfs_skurl_escaped_subject(subject)
    s = s.strip()
    return d + ' ' + s;

# This checks path and replace characters when needed so that the result is valid.
def kfs_convert_path_name(path_name):

    invalid_words = [
        "", "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
        "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    ]

    allowed_chars = [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    ]

    new_str = ""

    # Replace "#".
    path_name = path_name.replace("#", "#%03i" % ( ord("#") ) )

    # Replace bad words. Return immediately the converted string if a bad word is found.
    for invalid_word in invalid_words:
        if path_name == invalid_word:
            for char in path_name:
                new_str += "#%03i" % ( ord(char) )
            return new_str

    # Replace bad characters.
    for char in path_name:
        if allowed_chars[ord(char)]:
            new_str += char
        else:
            new_str += "#%03i" % ( ord(char) )

    # Replace bad leading characters.
    char = new_str[0:1]
    if char == " ":
        new_str = new_str[1:] + "#%03i" % ( ord(char) )

    # Replace bad trailing characters.
    char = new_str[-1:]
    if char == ".":
        new_str = new_str[:-1] + "#%03i" % ( ord(char) )

    return new_str

# This class represents a Web KFS node.
class WebKFSNode(kbase.PropStore):
    def __init__(self, workspace_id=None, share_id=None, inode_id=None):
        self.workspace_id = workspace_id
        self.share_id = share_id
        self.inode_id = inode_id

    def from_dict(self, d):
        self.workspace_id = d['workspace_id']
        self.share_id = d['share_id']
        self.inode_id = d['inode_id']
        return self

    def __str__(self):
        return "<%s ws_id=%s share_id=%s inode_id=%s>" % \
            ( self.__class__.__name__, str(self.workspace_id), str(self.share_id), str(self.inode_id) )

# This class represents a Web KFS directory.
class WebKFSDirectory(WebKFSNode):
    pass

# This class represents a Web KFS file.
class WebKFSFile(WebKFSNode):
    pass

# Represent a directory to delete (new style)
class KFSOpDirDelete(object):
    # Accessible attributes
    __slots__ = ['kfs_op', 'inode_id', 'commit_id', 'kfs_error']

    def __init__(self, inode_id, commit_id):
        self.kfs_op = kanp.KANP_KFS_OP_DELETE_DIR
        self.inode_id = inode_id
        self.commit_id = commit_id
        self.kfs_error = None

# Represent a file to delete (new style).
class KFSOpFileDelete(object):
    # Accessible attributes
    __slots__ = ['kfs_op', 'inode_id', 'commit_id', 'kfs_error']

    def __init__(self, inode_id, commit_id):
        self.kfs_op = kanp.KANP_KFS_OP_DELETE_FILE
        self.inode_id = inode_id
        self.commit_id = commit_id
        self.kfs_error = None 

# NOT USED #
if 0:
    # This class represents a KFS directory.
    class KFSDirectory(kbase.PropStore):
        def __init__(self):
            self.workspace_id = 0
            self.share_id = 0
            self.inode = 0
            self.parent_inode_id = 0
            self.commit_id = 0
            self.user_id = 0
            self.date = 0
            self.name = ''
            self.kfs_error = None

# This class represents a KFS file.
class KFSFile(kbase.PropStore):
    def __init__(self):
        self.workspace_id = 0
        self.share_id = 0
        self.inode = 0
        self.parent_inode_id = 0
        self.commit_id = 0
        self.user_id = 0
        self.date = 0
        self.size = 0
        self.hash = None
        self.name = ''

# NOT USED #
if 0:
    # This class handles writing to a file.
    class KFSFileWriter(object):
        def __init__(self, file_path):
            self._fd = None
            self.file_path = file_path
            log.debug("%s: instantiated with file path '%s'." % ( self.__class__.__name__, self.file_path ))
       
        def open(self):
            self._fd = os.open(self.file_path, os.O_RDWR|os.O_CREAT)
            log.debug("%s: opened file '%s'." % ( self.__class__.__name__, self.file_path )) 

        def write(self, data):
            os.write(self._fd, data)
            # Do not uncomment!
            #log.debug("%s: writing file %i bytes." % ( self.__class__.__name__, len(data) ))

        def close(self):
            os.close(self._fd)
            log.debug("%s: closed file '%s'." % ( self.__class__.__name__, self.file_path ))

# This class represents a KFS uploaded file.
class KFSUploadFile(KFSFile):
    def __init__(self):
        KFSFile.__init__(self)

        self.kfs_op = None
        self.fd = None
        self.chunks = []
        self.kfs_error = None

    # This method sets some attributes based on an open file descriptor.
    def set_from_fd(self, fd, size=None):
        self.chunks = []

        # Get hash of file.
        self.hash = "X"*16 #kfs_compute_hash(fd)

        # Set fd and size.
        self.fd = fd
        self.size = size
        if not size: self.size = os.fstat(fd)[stat.ST_SIZE]

        # Virtually split the file in chunks.
        offset=0
        while offset < self.size:
            remaining_bytes = self.size - offset
            size = min(remaining_bytes, KFS_CHUNK_SIZE)
            self.chunks += [KFSChunk(self.fd, offset, size)]
            offset += size

# NOT USED #
if 0:
    # This class represents a KFS downloaded file.
    class KFSDownloadFile(KFSFile):
        def __init__(self):
            KFSFile.__init__(self)

            self.hash = None
            self.comm = None
            self.kfs_error = None

# This class represents a KFS chunk.
class KFSChunk(object):
    def __init__(self, fd, offset, size):
        self.fd = fd
        self.offset = offset
        self.size = size

    def read(self):
        os.lseek(self.fd, self.offset, os.SEEK_SET)
        s = ''
        cur = 0
        while cur < self.size:
            remaining_bytes = self.size - cur
            d = os.read(self.fd, remaining_bytes)
            cur += len(d)
            s += d
        return s

    def __repr__(self):
        return "<%s fd=%i offset=%i size=%i>" % ( self.__class__.__name__, self.fd, self.offset, self.size )

class PhaseTwoCommitSubMessage(object):
    def __init__(self):
        self.size = 0
        self.anpm = None

class PhaseTwoChunkSubMessage(object):
    def __init__(self):
        self.size = 0
        self.anpm = None
        self.chunk = None

class PhaseTwoMessage(object):
    def __init__(self):
        self.size = 0
        self.sub_messages = []
        self.anpm = None

# This class handles KFS operations like creating and updating files in KCD.
class KFSOperations(object):
    def __init__(self, kfs_entries, reader, writer):
        self.kfs_entries = kfs_entries
        self.reader = reader
        self.writer = writer

        self.phase_two_messages = []

    # Allows creating and updating files (need phase 2) or creating directories. 
    def phase_one(self, email_id, ticket):

        # Prepare phase one ANP message.
        m = kanp.ANP_msg()
        m.add_bin(ticket)
        m.add_u64(email_id)
        m.add_u32(len(self.kfs_entries))
        for kfs_entry in self.kfs_entries:
            if kfs_entry.kfs_op == kanp.KANP_KFS_OP_CREATE_FILE:
                m.add_u32(5) # nb of elements
                m.add_u32(kfs_entry.kfs_op)
                m.add_u64(kfs_entry.parent_inode_id)
                m.add_u64(kfs_entry.parent_commit_id)
                m.add_str(kfs_entry.name)

            elif kfs_entry.kfs_op == kanp.KANP_KFS_OP_UPDATE_FILE:
                m.add_u32(4) # nb of elements
                m.add_u32(kfs_entry.kfs_op)
                m.add_u64(kfs_entry.inode)
                m.add_u64(kfs_entry.commit_id)

            elif kfs_entry.kfs_op == kanp.KANP_KFS_OP_CREATE_DIR:
                m.add_u32(5) # nb of elements
                m.add_u32(kfs_entry.kfs_op)
                m.add_u64(kfs_entry.parent_inode_id)
                m.add_u64(kfs_entry.parent_commit_id)
                m.add_str(kfs_entry.name)

            elif kfs_entry.kfs_op == kanp.KANP_KFS_OP_DELETE_DIR:
                m.add_u32(4) # nb of elements
                m.add_u32(kfs_entry.kfs_op)
                m.add_u64(kfs_entry.inode_id)
                m.add_u64(kfs_entry.commit_id)

            elif kfs_entry.kfs_op == kanp.KANP_KFS_OP_DELETE_FILE:
                m.add_u32(4) # nb of elements
                m.add_u32(kfs_entry.kfs_op)
                m.add_u64(kfs_entry.inode_id)
                m.add_u64(kfs_entry.commit_id)

            else:
                raise Exception("Unexpected KFS operation: '%s'." % ( str(kfs_entry.kfs_op) ) )

        # Send phase one ANP message to KCD.
        payload = m.get_payload()
        self.writer.send_command_header(kanp.KANP_CMD_KFS_PHASE_1, len(payload))
        self.writer.write(payload)
        log.debug("Phase 1 data sent.")

        # Get phase one result.
        h, m = kanp.get_anpt_all(self.reader)
        if h.type != kanp.KANP_RES_KFS_PHASE_1:
            assert h.type == kanp.KANP_RES_FAIL
            raise kanp.KANPFailure(m.get_u32(), m.get_str())
        log.debug("Got phase 1 reply.")

        # Handle phase one reply.
        phase_two_needed = False
        commit_id = m.get_u64()
        nb_op = m.get_u32()
        assert nb_op == len(self.kfs_entries)
        for i in range(0, nb_op):
            errno = m.get_u32()
            error = m.get_str()
            if error:
                log.debug(
                    "Phase 1: KFS operation %i error: errno=%i, error='%s'" % \
                    ( i, errno, error ))
                self.kfs_entries[i].kfs_error = error

    # This function prepares anp messages and sub-messages for phase_two().
    # Knowing in advance the size of the files is needed for this function. See other methods for asynchronous uploads.
    # NOTE: No longer used, might not be fully working.
    def prepare_phase_two(self):
        message = None
        files_iter = iter(self.kfs_entries)
        switch_file = True
        switch_message = True
        commit_file = False
        switch_chunk = True
        exit = False

        while 1:

            if exit or switch_message:
                switch_message = False

                if message and len(message.sub_messages) > 0:
                    # Finish ANPT message preparation.
                    message.anpm = kanp.ANP_msg()
                    message.anpm.add_u32(len(message.sub_messages))
                    message.size += message.anpm.get_payload_size()
                
                    # Append ANPT message to list.
                    self.phase_two_messages.append(message)

                # Init new ANPT message.
                message = PhaseTwoMessage()

            if exit:
                break

            if commit_file:
                commit_file = False

                # Prepare a file commit sub-message.
                log.debug("Committing file.")

                # Prepare a partial anp message (missing an ANP bin field for the MD5 signature of the file).
                subm = PhaseTwoCommitSubMessage()
                subm.anpm = kanp.ANP_msg()
                subm.anpm.add_u32(3)
                subm.anpm.add_u32(kanp.KANP_KFS_SUBMESSAGE_COMMIT)
                #hash = kfs_compute_hash(kfs_entry.fd)
                #subm.anpm.add_bin(kfs_entry.hash)

                # Calculate total sub-message size.
                subm.size = subm.anpm.get_payload_size() + 5 + 16 # partial anp mesg + anp bin header + md5 sign.
                log.debug("Commit sub-message has %i bytes in total." % ( subm.size ))

                # Append sub-message to current ANPT message.
                log.debug("Appending commit sub-message to ANPT message.")
                message.sub_messages.append(subm)
                message.size += subm.size

                # Switch to next file.
                switch_file = True
                continue


            if not message:
                # Init new message.
                log.debug("Initiating a new message.")
                message = PhaseTwoMessage()

            if switch_file:
                switch_file = False
                
                try:
                    # Get next file.
                    kfs_entry = files_iter.next()
                    log.debug("Got new file: '%s'." % ( kfs_entry.name ))

                    # Start again with file chunk.
                    chunks_iter = iter(kfs_entry.chunks)
                    switch_chunk = True
                    continue

                except StopIteration:
                    # No more file in list.
                    log.debug("No more file.")

                    exit = True
                    continue

            if kfs_entry.kfs_op != kanp.KANP_KFS_OP_CREATE_FILE and kfs_entry.kfs_op != kanp.KANP_KFS_OP_UPDATE_FILE:
                # That operation does not need any phase 2 messsage.
                log.debug("No phase two needed for that operation.")
                switch_file = True
                continue

            if kfs_entry.kfs_error:
                # This file cannot be uploaded. Pass to next file.
                log.debug("Skipping file '%s' because it had an error in phase 1: '%s'." % \
                    (kfs_entry.name, kfs_entry.kfs_error ))
                switch_file = True
                continue

            if switch_chunk:
                switch_chunk = False

                try:
                    # Get next KFS file chunk.
                    chunk = chunks_iter.next()
                    log.debug("Got a new chunk of %i bytes." % ( chunk.size ))

                except StopIteration:
                    # No more chunks. Commit file.
                    commit_file = True
                    continue
               
            # Add chunk to current ANPT message.

            # Prepare a partial anp message (missing an ANP bin field for the chunk data).
            subm = PhaseTwoChunkSubMessage()
            subm.anpm = kanp.ANP_msg()
            subm.anpm.add_u32(3)
            subm.anpm.add_u32(kanp.KANP_KFS_SUBMESSAGE_CHUNK)
            #subm.anpm.add_bin(chunk.read())

            # Set sub-message chunk.
            subm.chunk = chunk

            # Calculate total sub-message size.
            subm.size = subm.anpm.get_payload_size() + 5 + chunk.size # partial anp mesg + anp bin header + chunk data
            log.debug("Chunk sub-message has %i bytes in total." % ( subm.size ))

            if (message.size + subm.size + 100000) > kanp.ANPT_MSG_MAX_SIZE:
                # Current ANPT message cannot accept chunk.
            
                # Switch ANPT message.        
                switch_message = True
                # Do not switch chunk (implicit).
                #switch_chunk = False
                continue

            # Append sub-message to this message.
            log.debug("Appending chunk sub-message to ANPT message.")
            message.sub_messages.append(subm)
            message.size += subm.size

            switch_chunk = True

    # This function handles the phase two communications, after messages are prepared in prepare_phase_two().
    # NOTE: No longer used, might not be fully working.
    def phase_two(self):

        hash = None
        i = -1
        for message in self.phase_two_messages:
            i += 1

            # Sent ANP transport header
            log.debug("Phase 2: sending ANPT header %i, size %i." % ( i, message.size ))
            self.writer.send_command_header(kanp.KANP_CMD_KFS_PHASE_2, message.size)
            log.debug("Phase 2: sent ANPT header %i, size %i." % ( i, message.size ))

            # Send base message anp message.

            kanp.send_anpt_msg(self.writer, message.anpm)

            if not hash:
                hash = hashlib.md5()

            j = -1
            for subm in message.sub_messages:
                j += 1
                if isinstance(subm, PhaseTwoChunkSubMessage):
                    # send chunk
                    log.debug("Phase 2: preparing file %i chunk %i anp message." % ( i, j ))
                    bytes = subm.chunk.read()
                    hash.update(bytes)
                    subm.anpm.add_bin(bytes)
                    log.debug("Phase 2: sending file %i chunk %i anp message." % ( i, j ))
                    kanp.send_anpt_msg(self.writer, subm.anpm)
                    log.debug("Phase 2: sent file %i chunk %i anp message." % ( i, j ))

                else:
                    assert isinstance(subm, PhaseTwoCommitSubMessage)
                    # send commit
                    log.debug("Phase 2: preparing file %i commit anp message." % ( i ))
                    bytes = hash.digest()
                    subm.anpm.add_bin(bytes)
                    hash = hashlib.md5()
                    log.debug("Phase 2: sending file %i commit anp message." % ( i ))
                    kanp.send_anpt_msg(self.writer, subm.anpm)
                    log.debug("Phase 2: sent file %i commit anp message." % ( i ))

            # get response
            log.debug("Phase 2: getting %i reply." % ( i ))
            h, m = kanp.get_anpt_all(self.reader)
            log.debug("Phase 2: got %i reply." % ( i ))
            if h.type == kanp.KANP_RES_FAIL:
                raise kanp.KANPFailure(m.get_u32(), m.get_str())
            assert h.type == kanp.KANP_RES_OK

        # get response
        h, m = kanp.get_anpt_all(self.reader)
        log.debug("Phase 2: got final reply.")
        if h.type == kanp.KANP_RES_FAIL:
            raise kanp.KANPFailure(m.get_u32(), m.get_str())
        assert h.type == kanp.KANP_RES_OK

        log.debug("File upload finished.")

        #return kfs_entries

    # Create a phase 2 chunk sub-message.
    def phase_2_create_chunk_submessage(self, data):
        # Prepare anp message
        subm = PhaseTwoChunkSubMessage()
        subm.anpm = kanp.ANP_msg()
        subm.anpm.add_u32(3)
        subm.anpm.add_u32(kanp.KANP_KFS_SUBMESSAGE_CHUNK)
        subm.anpm.add_bin(data)
        return subm

    # Create a phase 2 commit sub-message.
    def phase_2_create_commit_submessage(self, hash):
        subm = PhaseTwoCommitSubMessage()
        subm.anpm = kanp.ANP_msg()
        subm.anpm.add_u32(3)
        subm.anpm.add_u32(kanp.KANP_KFS_SUBMESSAGE_COMMIT)
        subm.anpm.add_bin(hash)
        return subm

    # Send a phase 2 message with only 1 submessage
    # (for asynchronous uploads when file(s) size(s) is/are not yet known...).
    def phase_2_send_message_with_one_submessage(self, subm):

        # Prepare ANP message.
        message = PhaseTwoMessage()
        message.anpm = kanp.ANP_msg()
        message.anpm.add_u32(1) # Send only one sub-message

        # Calculate base messasge size.
        message.size = message.anpm.get_payload_size()
        #log.debug("Base message size: %i bytes." % ( message.size ))

        # Calculate total sub-message size.
        subm.size = subm.anpm.get_payload_size()
        log.debug("Chunk sub-message size: %i bytes." % ( subm.size ))

        total_size = message.size + subm.size

        # Sent ANP transport header
        #log.debug("Phase 2: sending ANPT header with data size %i." % ( total_size ))
        self.writer.send_command_header(kanp.KANP_CMD_KFS_PHASE_2, total_size)
        #log.debug("Phase 2: sent ANPT header, size %i." % ( total_size ))

        # Send base message.
        kanp.send_anpt_msg(self.writer, message.anpm)

        # Send sub-message.
        kanp.send_anpt_msg(self.writer, subm.anpm)

        # get response
        #log.debug("Phase 2: getting reply.")
        h, m = kanp.get_anpt_all(self.reader)
        #log.debug("ANP RESPONSE DUMP: %s" % (str(m.dump())))
        #log.debug("Phase 2: got reply.")
        if h.type == kanp.KANP_RES_FAIL:
            raise kanp.KANPFailure(m.get_u32(), m.get_str())
        assert h.type == kanp.KANP_RES_OK

def kfs_compute_hash(fd):
    os.lseek(fd, 0, 0)
    hash = hashlib.md5()

    while 1:
        data = os.read(fd, 1024*1024)
        if len(data) == 0: break
        hash.update(data)

    return hash.digest()

