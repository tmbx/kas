#!/usr/bin/python

# from system
import struct, logging

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# ANP element types
ANP_U32 = 1
ANP_U64 = 2
ANP_STR = 3
ANP_BIN = 4
ANP_TYPES = {ANP_U32 : 'U32', ANP_U64 : 'U64', ANP_STR : 'STR', ANP_BIN : 'BIN'}

# ANP transport
ANPT_HEADER_SIZE = 4*4+8*1
ANPT_MSG_MAX_SIZE = (100*1024*1024)

# KANP basics
KANP_PROTO = (1 << 28)

KANP_CMD = (0 << 26)
KANP_RES = (1 << 26)
KANP_EVT = (2 << 26)

KANP_NS_GEN = (0 << 16)
KANP_NS_MGT = (1 << 16)
KANP_NS_KWS = (2 << 16)
KANP_NS_CHAT = (4 << 16)
KANP_NS_KFS = (5 << 16)
KANP_NS_VNC = (6 << 16)
KANP_NS_WB = (7 << 16)
KANP_NS_PB = (8 << 16)

# KANP version
KANP_MAJOR = 0
KANP_MINOR = 4

# KFS submessage identifiers
KANP_KFS_SUBMESSAGE_FILE = 1
KANP_KFS_SUBMESSAGE_CHUNK = 2
KANP_KFS_SUBMESSAGE_COMMIT = 3
KANP_KFS_SUBMESSAGE_ABORT = 4

# KFS operation identifiers
KANP_KFS_OP_CREATE_FILE = 1
KANP_KFS_OP_CREATE_DIR = 2
KANP_KFS_OP_UPDATE_FILE = 3
KANP_KFS_OP_DELETE_FILE = 4
KANP_KFS_OP_DELETE_DIR = 5
KANP_KFS_OP_MOVE_FILE = 6
KANP_KFS_OP_MOVE_DIR = 7

# KFS entry type identifiers
KANP_KFS_ENTRY_FILE = 1
KANP_KFS_ENTRY_DIR = 2

# Workspace flags.
KANP_KWS_FLAG_PUBLIC = (1 << 0)
KANP_KWS_FLAG_FREEZE = (1 << 1)
KANP_KWS_FLAG_DEEP_FREEZE = (1 << 2)
KANP_KWS_FLAG_THIN_KFS = (1 << 3)
KANP_KWS_FLAG_SECURE = (1 << 4)
KANP_KWS_FLAG_DELETE = (1 << 30)
KANP_KWS_FLAG_COMPAT_V2 = (1 << 29)

# User flags.
KANP_USER_FLAG_ADMIN = (1 << 0)
KANP_USER_FLAG_MANAGER = (1 << 1)
KANP_USER_FLAG_REGISTER = (1 << 2)
KANP_USER_FLAG_LOCK = (1 << 3)
KANP_USER_FLAG_BAN = (1 << 4)
KANP_USER_FLAG_ROOT = (1 << 30) # unpublished

# Property change types.
KANP_PROP_KWS_NAME = 1
KANP_PROP_KWS_FLAGS = 2
KANP_PROP_USER_NAME_ADMIN = 101
KANP_PROP_USER_NAME_USER = 102
KANP_PROP_USER_FLAGS = 103

# KANP ticket types
KANP_KCD_TICKET_DOWNLOAD = 1
KANP_KCD_TICKET_UPLOAD = 2
KANP_KCD_TICKET_VNC_SERVER = 3
KANP_KCD_TICKET_VNC_CLIENT = 4

# KANP public workspaces commands
KANP_CMD_PB_ACCEPT_CHAT = (KANP_PROTO | KANP_CMD | KANP_NS_PB | (1 << 8))

# KANP public workspaces events
KANP_EVT_PB_TRIGGER_CHAT = (KANP_PROTO | KANP_EVT | KANP_NS_PB | (1 << 8))
KANP_EVT_PB_CHAT_ACCEPTED = (KANP_PROTO | KANP_EVT | KANP_NS_PB | (2 << 8))
KANP_EVT_PB_TRIGGER_KWS = (KANP_PROTO | KANP_EVT | KANP_NS_PB | (3 << 8))

# KANP misc
KANP_RES_OK = 335544320
KANP_RES_FAIL = 335544576
KANP_RES_FAIL_GEN = 0
KANP_RES_FAIL_BACKEND = 1
KANP_RES_FAIL_CHOOSE_USER_ID = 2
KANP_RES_FAIL_EVT_OUT_OF_SYNC = 3
KANP_RES_FAIL_MUST_UPGRADE = 4

KANP_RES_FAIL_PERM_DENIED  = 5
KANP_RES_FAIL_FILE_QUOTA_EXCEEDED = 6
KANP_RES_FAIL_RESOURCE_QUOTA = 7

KANP_RESOURCE_QUOTA_GENERAL = 0         
KANP_RESOURCE_QUOTA_NO_SECURE = 1

KANP_CMD_MGT_SELECT_ROLE = 268500992
KANP_KCD_ROLE_WORKSPACE = 1
KANP_KCD_ROLE_FILE_XFER = 2
KANP_KCD_ROLE_APP_SHARE = 3
KANP_CMD_MGT_CREATE_KWS = 268501248
KANP_RES_MGT_KWS_CREATED = 335610112
KANP_CMD_KWS_INVITE_KWS = 268567040
KANP_RES_KWS_INVITE_KWS = 335675904
KANP_CMD_KWS_CONNECT_KWS = 268567296
KANP_RES_KWS_CONNECT_KWS = 335676160
KANP_CMD_KWS_DISCONNECT_KWS = 268567552
KANP_CMD_CHAT_MSG = 268697856
KANP_CMD_KFS_DOWNLOAD_REQ = 268763392
KANP_RES_KFS_DOWNLOAD_REQ = 335872256
KANP_CMD_KFS_UPLOAD_REQ = 268763648
KANP_RES_KFS_UPLOAD_REQ = 335872512
KANP_CMD_KFS_DOWNLOAD_DATA = 268763904
KANP_RES_KFS_DOWNLOAD_DATA = 335872768
KANP_CMD_KFS_PHASE_1 = 268764160
KANP_RES_KFS_PHASE_1 = 335873024
KANP_CMD_KFS_PHASE_2 = 268764416
KANP_CMD_VNC_START_TICKET = 268828928
KANP_RES_VNC_START_TICKET = 335937792
KANP_CMD_VNC_START_SESSION = 268829184
KANP_CMD_VNC_CONNECT_TICKET = 268829440
KANP_RES_VNC_CONNECT_TICKET = 335938304
KANP_CMD_VNC_CONNECT_SESSION = 268829696
KANP_CMD_WB_DRAW = 268894464
KANP_CMD_WB_CLEAR = 268894720
KANP_EVT_KWS_PROP_CHANGE = (KANP_PROTO | KANP_EVT | KANP_NS_KWS | (6 << 8))
KANP_EVT_KWS_CREATED = 402784512
KANP_EVT_KWS_INVITED = 402784768
KANP_EVT_KWS_USER_REGISTERED = 402785024
KANP_EVT_CHAT_MSG = 402915584
KANP_EVT_KFS_PHASE_1 = 402981120
KANP_EVT_KFS_PHASE_2 = 402981376
KANP_EVT_VNC_START = 403046656
KANP_EVT_VNC_END = 403046912
KANP_EVT_WB_DRAW = 403112192
KANP_EVT_WB_CLEAR = 403112448


# Send freemium confirmartion email
KANP_CMD_MGT_FREEMIUM_CONFIRM = (KANP_PROTO | KANP_CMD | KANP_NS_MGT | (2 << 8))

# Login result codes.

# The login credentials are accepted.
KANP_KWS_LOGIN_OK =1

# The credentials are accepted but the login failed since the information
# about the last event received is invalid, probably because the server
# crashed and lost some events. All events must be refetched from the server.

KANP_KWS_LOGIN_OOS = 2

#The password and/or the ticket are refused.
KANP_KWS_LOGIN_BAD_PWD_OR_TICKET = 3

# The workspace ID is invalid.
KANP_KWS_LOGIN_BAD_KWS_ID = 4

# The email ID is invalid or it has been purged from the database.
KANP_KWS_LOGIN_BAD_EMAIL_ID = 5

# The workspace has been deleted.
KANP_KWS_LOGIN_DELETED_KWS = 6

# The user account has been locked.
KANP_KWS_LOGIN_ACCOUNT_LOCKED = 7

# Notification and summary emails flags
KANP_EMAIL_NOTIF_FLAG = 1
KANP_EMAIL_SUMMARY_FLAG = 2       

# This class represents an ANP message.
class ANP_msg(object):
    def __init__(self):
        self.id = 0
        self.type = 0
        self.id_list = []
        self.val_list = []
        self.last_offset = -1
    
    # This method parses the data specified.
    def parse(self, data):
        self.last_offset = -1
        cur = 0
        l = len(data)
        
        while cur < l:
            t = struct.unpack(">b", data[cur])[0]
            cur += 1
            
            if t == ANP_U32:
                v = struct.unpack(">i", self._parse_range(data, cur, 4))[0]
                cur += 4
                self.id_list.append(ANP_U32)
                self.val_list.append(v)
            
            elif t == ANP_U64:
                v = struct.unpack(">Q", self._parse_range(data, cur, 8))[0]
                cur += 8
                self.id_list.append(ANP_U64)
                self.val_list.append(v)
                
            elif t == ANP_STR:
                vl = struct.unpack(">i", self._parse_range(data, cur, 4))[0]
                cur += 4
                v = self._parse_range(data, cur, vl)
                cur += vl
                self.id_list.append(ANP_STR)
                self.val_list.append(v)
    
            elif t == ANP_BIN:
                vl = struct.unpack(">i", self._parse_range(data, cur, 4))[0]
                cur += 4
                v = self._parse_range(data, cur, vl)
                cur += vl
                self.id_list.append(ANP_BIN)
                self.val_list.append(v)

        return self

    # This method dumps a list of tuples containing:
    #   full: (string type, size, hex encoded data)
    #   not full: (string type, size)
    def dump(self, full=False):
        l = [] 
        for i in range(0, len(self.id_list)):
            t = self.id_list[i]
            v = self.val_list[i]

            # Get the string version of the type.
            type_str = ANP_TYPES[t]

            # Get the size.
            if t == ANP_U32:
                size = 4
                if full: value = v

            elif t == ANP_U64:
                size = 8
                if full: value = v

            elif t == ANP_STR:
                size = 4 + len(v)
                if full: value = v

            elif t == ANP_BIN:
                try:
                    size = 4 + v.length()
                    if full: value = v.read()
                except AttributeError:
                    size = 4 + len(v)
                    if full: value = v.encode('hex')

            else:
                raise Exception("Invalid ANP type: %i" % (t) )

            if full:
                l.append((type_str, size, value))
            else:
                l.append((type_str, size))

        return l
 
    # This method gets the payload size of the message.
    def get_payload_size(self):
        size = 0
        
        for i in range(0, len(self.id_list)):
            t = self.id_list[i]
            v = self.val_list[i]

            size += 1
            
            if t == ANP_U32:
                size += 4
            
            elif t == ANP_U64:
                size += 8
                
            elif t == ANP_STR:
                size += 4 + len(v)
    
            elif t == ANP_BIN:
                try:
                    size += 4 + v.length()
                except AttributeError:
                    size += 4 + len(v)

            else:
                raise Exception("Invalid ANP type: %i" % (t) )
        
        return size
    
    # This method returns a byte array representing the message payload.
    def get_payload(self):
        payload = ""
        
        for i in range(0, len(self.id_list)):
            t = self.id_list[i]
            v = self.val_list[i]
            
            payload += struct.pack(">b", t)
            
            if t == ANP_U32:
                payload += struct.pack(">i", v)
            
            elif t == ANP_U64:
                payload += struct.pack(">Q", v)
                
            elif t == ANP_STR:
                payload += struct.pack(">i", len(v))
                payload += v
    
            elif t == ANP_BIN:
                payload += struct.pack(">i", len(v))
                try:
                    payload += v.read()
                except AttributeError:
                    payload += v

            else:
                raise Exception("Invalid ANP type: %i" % (t) )
            
        return payload
    
    # This method returns an array of bytes at the offset specified with the
    # length specified.
    def _parse_range(self, data, cur, nb):
        if cur + nb > len(data): raise Exception("invalid ANP message format")
        return data[cur : cur + nb]
        
    # This method returns the UINT32 element at the offset specified.
    def get_u32(self, offset=None):
        return self._get_element(offset, ANP_U32)
    
    # This method returns the UINT64 element at the offset specified.
    def get_u64(self, offset=None):
        return self._get_element(offset, ANP_U64)
    
    # This method returns the string element at the offset specified.
    def get_str(self, offset=None):
        return unicode(self._get_element(offset, ANP_STR), encoding='latin1', errors='replace')
        
    # This method returns the binary element at the offset specified.
    def get_bin(self, offset=None):
        return self._get_element(offset, ANP_BIN)
    
    # This method adds an UINT32 element.
    def add_u32(self, value):
        self._add_element(ANP_U32, value)
        
    # This method adds an UINT64 element.
    def add_u64(self, value):
        self._add_element(ANP_U64, value)
        
    # This method adds a string element.
    def add_str(self, value):
        self._add_element(ANP_STR, value.encode('latin1', 'replace'))
        
    # This method adds a binary element.
    def add_bin(self, value):
        self._add_element(ANP_BIN, value)
    
    # Helper method.
    def _get_element(self, offset, type):
        if not offset: offset  = self.last_offset + 1
        self.last_offset = offset
        if offset < 0 or offset >= self.id_list:
            raise Exception("invalid ANP element offset")
        if self.id_list[offset] != type:
            raise Exception("invalid ANP element type (expected %i, seen %i)" % (type, self.id_list[offset]))
        return self.val_list[offset]

    # Helper method.
    def _add_element(self, t, value):
        if t == ANP_U32:
            if value < 0 or value >= 2**32:
                raise Exception("Invalid value for U32: '%s'" % ( str(value) ) )
        elif t == ANP_U64:
            if value < 0 or value >= 2**64:
                raise Exception("Invalid value for U64: '%s'" % ( str(value) ) )
        elif t == ANP_STR:
            if type(value) not in [str, unicode]:
                raise Exception("Invalid value for STR: '%s'" % ( str(value) ) )
        elif t == ANP_BIN:
            if type(value) != str:
                raise Exception("Invalid value for BIN: '%s'" % ( str(value) ) )
        self.id_list.append(t)
        self.val_list.append(value)
        
        # Do not uncomment!
        #log.debug("ANP payload size is now %i." % ( self.get_payload_size() ))

# This class represents an ANP transport header.
class ANPT_header:
    def __init__(self, type=None, id=None, size=None):
        self.major = KANP_MAJOR
        self.minor = KANP_MINOR
        self.type = type
        self.id = id
        self.size = size

    def parse(self, data):
        self.major = struct.unpack(">i", data[0:4])[0]
        self.minor = struct.unpack(">i", data[4:8])[0]
        self.type = struct.unpack(">i", data[8:12])[0]
        self.id = struct.unpack(">Q", data[12:20])[0]
        self.size = struct.unpack(">i", data[20:24])[0]
        return self

    def get_payload(self):
        p = ''
        p += struct.pack(">i", self.major)
        p += struct.pack(">i", self.minor)
        p += struct.pack(">i", self.type)
        p += struct.pack(">Q", self.id)
        p += struct.pack(">i", self.size)
        return p

# This method reads an anp message from connection.
def get_anp_msg(reader, size):
    log.debug("get_anp_msg() called")

    data = reader.read(size)
    # Do not uncomment!
    #log.debug("Got anp message: hex: '%s'." % ( data.encode('hex') ))
    #log.debug("Got anp message of %i bytes." % ( len(data) ))

    m = ANP_msg().parse(data)
    return m

# This method reads an anp transport header from reader.
def get_anpt_header(reader):
    log.debug("get_anpt_header() called")

    data = reader.read(ANPT_HEADER_SIZE)
    # Do not uncomment!
    #log.debug("Got anp transport header: hex: '%s'." % ( data.encode('hex') ))
    #log.debug("Got anp transport header of '%i' bytes." % ( len(data) ))

    h = ANPT_header().parse(data)
    return h

# This method gets a response header and message from connection. It returns
# a tuple (header, message).
def get_anpt_all(reader):
    log.debug("get_anpt_all() called")

    h = get_anpt_header(reader)
    if h.size: return h, get_anp_msg(reader, h.size)
    return h, ANP_msg()

# This method sends an anp transport header to writer.
def send_anpt_header(writer, type, id, size):
    log.debug("send_anpt_header() called")

    assert size <= ANPT_MSG_MAX_SIZE

    h = ANPT_header()
    h.type = type
    h.id = id
    h.size = size
    payload = h.get_payload()
    bytes = writer.write(payload)
    assert bytes == len(payload)
    # Do not uncomment!
    #log.debug("Sent anp transport header: hex: '%s'." % ( payload.encode('hex') ))
    #log.debug("Sent anp transport header of %i bytes." % ( len(payload) ))

    return bytes

# This method sends an anp transport header to writer.
def send_anpt_msg(writer, anp_msg):
    log.debug("send_anpt_msg() called")

    payload_size = anp_msg.get_payload_size()
    payload = anp_msg.get_payload()
    bytes = writer.write(payload)
    assert bytes == payload_size
    # Do not uncomment!
    #log.debug("Sent anp transport message: hex: '%s'." % ( payload.encode('hex') ))
    #log.debug("Sent anp transport message of %i bytes." % ( bytes ))

    return bytes

# This class represents a KANP failure.
class KANPFailure(Exception):
    def __init__(self, errno, err):
        self.errno = errno
        self.err = err
        log.debug("KANPFailure(): errno=%i, error='%s'." % ( errno, err ))

    def __str__(self):
        return repr(self.err)

# Load dir() in a global variable.
glob_dir = dir()

# This function returns a dictionary with all kanp constants from the glob_dir global variable.
def get_kanp_constants():
    constants = {}
    for e in glob_dir:
        if e.startswith("KANP_"):
            constants[e] = eval(e)
    return constants

# This function finds the matching constant name for its value.
def get_kanp_constant(value, startswith=None):
    constants = {}
    for e in glob_dir:
        if e.startswith("KANP_"):
            if (not startswith or e.startswith(startswith)) and eval(e) == value:
                return e
    return None

# This function checks if parameter is a number and fits in an unsigned 32 bits integer.
def isUInt32(v):
    try:
        if v.isdigit() and v >= 0 and v < (2**32): return True
        return False
    except Exception:
        return False

# This function checks if parameter is a number and fits in an unsigned 64 bits integer.
def isUInt64(v):
    try:
        if v.isdigit() and v >= 0 and v < (2**64): return True
        return False
    except Exception:
        return False

# When executed, this module lists all (or specific) KANP constants and their value.
# This is for debugging only. Do not use in production.
def main():
    import sys
    kanp_constants = get_kanp_constants()
    args = sys.argv[1:]
    if len(args) == 0:
        for n in kanp_constants:
            print n + "=" + str(kanp_constants[n])
    else:
        for n in args:
            print n + "=" + str(kanp_constants[n])

if __name__ == "__main__":
    main()

