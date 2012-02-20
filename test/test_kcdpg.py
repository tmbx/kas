#!/usr/bin/python

import sys, os, time, ConfigParser, random, re, struct
from kpg import *

# To run Postgres under valgrind, run the following command and check
# /var/log/daemon:
# valgrind --tool=memcheck --gen-suppressions=no --leak-check=yes --num-callers=15 --trace-children=yes /usr/bin/pg_ctlcluster 8.4 teambox start

# Protocol constants
ANP_U32 = 1
ANP_U64 = 2
ANP_STR = 3
ANP_BIN = 4

KANP_RES_OK = 335544320
KANP_RES_FAIL = 335544576
KANP_RES_FAIL_GEN = 0
KANP_RES_FAIL_BACKEND = 1
KANP_RES_FAIL_CHOOSE_USER_ID = 2
KANP_RES_FAIL_EVT_OUT_OF_SYNC = 3
KANP_RES_FAIL_MUST_UPGRADE = 4
KANP_CMD_MGT_SELECT_ROLE = 268500992
KANP_CMD_MGT_CREATE_KWS = 268501248
KANP_RES_MGT_KWS_CREATED = 335610112
KANP_CMD_MGT_INVITE_KWS = 268567040
KANP_RES_MGT_INVITE_KWS = 335675904
KANP_CMD_MGT_CONNECT_KWS = 268567296
KANP_RES_MGT_CONNECT_KWS = 335676160
KANP_CMD_MGT_DISCONNECT_KWS = 268567552
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

# This class represents an ANP message.
class ANP_msg:
    def __init__(self):
        self.id = 0
        self.type = 0
        self.id_list = []
        self.val_list = []
    
    # This method parses the data specified.
    def parse(self, data):
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
                payload += v
            
        return payload
    
    # This method returns an array of bytes at the offset specified with the
    # length specified.
    def _parse_range(self, data, cur, nb):
        if cur + nb > len(data): raise Exception("invalid ANP message format")
        return data[cur : cur + nb]
        
    # This method returns the UINT32 element at the offset specified.
    def get_u32(self, offset):
        return self._get_element(offset, ANP_U32)
    
    # This method returns the UINT64 element at the offset specified.
    def get_u64(self, offset):
        return self._get_element(offset, ANP_U64)
    
    # This method returns the string element at the offset specified.
    def get_str(self, offset):
        return self._get_element(offset, ANP_STR)
        
    # This method returns the binary element at the offset specified.
    def get_bin(self, offset):
        return self._get_element(offset, ANP_BIN)
    
    # This method adds an UINT32 element.
    def add_u32(self, value):
        self._add_element(ANP_U32, value)
        
    # This method adds an UINT64 element.
    def add_u64(self, value):
        self._add_element(ANP_U64, value)
        
    # This method adds a string element.
    def add_str(self, value):
        self._add_element(ANP_STR, value)
        
    # This method adds a binary element.
    def add_bin(self, value):
        self._add_element(ANP_BIN, value)
    
    # Helper method.
    def _get_element(self, offset, type):
        if offset < 0 or offset >= self.id_list:
            raise Exception("invalid ANP element offset")
        if self.id_list[offset] != type:
            raise Exception("invalid ANP element type (expected %i, seen %i)" % (type, self.id_list[offset]))
        return self.val_list[offset]

    # Helper method.
    def _add_element(self, type, value):
        self.id_list.append(type)
        self.val_list.append(value)

def exec_query(db, pg_func, arg, post):
    
    # Execute the query.
    cur = exec_pg_query(db, "SELECT %s(%s)" % (pg_func, escape_pg_bytea(arg.get_payload())))
    row = cur.fetchone()
    if not row:
        print "No row"
        return
    
    # Get the returned buffer.
    ret = ANP_msg()
    ret.parse(row[0].value)
    
    # Commit the transaction.
    db.commit()
    
    if post == None:
        print "Exec success"
        return
    
    # Get the return code.
    code = ret.get_u32(0);
    
    # Failure.
    if code == 1:
        error_msg = ret.get_str(1);
        print "Failure: " + error_msg
        return
    
    # Unexpected return code.
    if code != 0:
        print "Bad return code"
        return
    
    # Parse the result.
    post(ret)


def do_test_add_kws(db):
    
    # Prepare the ticket.
    ticket = ANP_msg()
    
    # Prepare the add_kws argument.
    arg = ANP_msg()
    arg.add_u32(0) # Status
    arg.add_u64(0) # Creation date.
    arg.add_str('some kws') # Name.
    arg.add_bin(ticket.get_payload()) # Ticket.
    
    # Execute the query.
    exec_query(db, "add_kws", arg, None)


def do_test_phase_one(db, tester):

    # Prepare the user command.
    cmd = ANP_msg()
    cmd.add_bin('') # Ticket.
    
    # Add the changes.
    tester(cmd)
    
    # Prepare the phase 1 argument.
    arg = ANP_msg()
    arg.add_u64(1)  # Workspace ID.
    arg.add_u32(0)  # User ID.
    arg.add_u32(0)  # Share ID.
    arg.add_bin(cmd.get_payload()) # User command payload.
    
    # Execute the query.
    exec_query(db, "upload_phase_one", arg, phase_one_success)

def phase_one_success(ret):
    commit_id = ret.get_u64(1)
    reply = ANP_msg()
    reply.parse(ret.get_bin(2))
    
    print "Success."
    print "Commit ID %i\n" % (commit_id)
    print ""
    print_phase_one_reply(reply)
    print ""
    print_phase_one_upload(ret)

def print_phase_one_reply(reply):
    print "Reply:"
    print "Commit ID: %i" % (reply.get_u64(0))
    nb_change = reply.get_u32(1)
    print "Number of changes: %i" % (nb_change)
    index = 2
    for i in range(0, nb_change):
        print "Operation %i: %i (%s)" % (i, reply.get_u32(index + 0), reply.get_str(index + 1))
        index += 2
    
def print_phase_one_upload(ret):
    nb_upload = ret.get_u32(3)
    print "Number of uploads: %i" % (nb_upload)
    index = 4
    for i in range(0, nb_upload):
        inode = ret.get_u64(index + 0)
        path = ret.get_str(index + 1)
        index += 2
        print "Upload %i: inode %i, path %s" % (i, inode, path)

def test_phase_one_batch_one(cmd):
    cmd.add_u32(10)  # Nb change.
    
    # Create file 'foo.txt'.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('foo.txt') # Entry path.
    
    # Create directory 'dir1'.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(2)  # Create dir.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('dir1') # Entry path.
    
    # Create directory 'dir1/dir2'.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(2)  # Create dir.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('dir1/dir2') # Entry path.
    
    # Create file 'dir1/file2.txt'.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('dir1/file2.txt') # Entry path.
    
    # Fail create file 'dir3/file.txt' (dir does not exist).
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('dir3/file.txt') # Entry path.
    
    # Fail create file 'foo.txt/file.txt' (component is not dir).
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('foo.txt/file.txt') # Entry path.
    
    # Fail create file 'dir1/file4.txt' (stale directory).
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(1)  # Commit ID.
    cmd.add_str('foo.txt/file.txt') # Entry path.
    
    # Fail create file '???/file.txt' (bad parent inode).
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(66) # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('file.txt') # Entry path.
    
    # Create file 'dir1/file4.txt.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(1)  # Create file.
    cmd.add_u64(2)  # Parent inode.
    cmd.add_u64(1)  # Commit ID.
    cmd.add_str('file4.txt') # Entry path.
    
    # Create dir dir4.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(2)  # Create dir.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('dir4') # Entry path.
    
# End result so far:
# file foo.txt: inode 1, parent 0, commit 1.
# dir dir1: inode 2, parent 0, commit 1.
# dir dir1/dir2: inode 3, parent 2, commit 1.
# file dir1/file2.txt: inode 4, parent 2, commit 1.
# file dir1/file4.txt: inode 5, parent 2, commit 1.
# dir dir4: inode 6, parent 0, commit 1.
def test_phase_one_batch_two(cmd):
    cmd.add_u32(4)  # Nb change.
    
    # Update file dir1/file2.txt.
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(3)  # Update file.
    cmd.add_u64(4)  # Inode to update.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail update file ??? (bad inode).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(3)  # Update file.
    cmd.add_u64(66) # Inode to update.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail update dir (is dir).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(3)  # Update file.
    cmd.add_u64(2)  # Inode to update.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail update file foo.txt (stale commit ID).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(3)  # Update file.
    cmd.add_u64(5)  # Inode to update.
    cmd.add_u64(2)  # Inode commit ID.

# End result so far:
# file foo.txt: inode 1, parent 0, commit 1.
# dir dir1: inode 2, parent 0, commit 1.
# dir dir1/dir2: inode 3, parent 2, commit 1.
# file dir1/file2.txt: inode 4, parent 2, commit 2.
# file dir1/file4.txt: inode 5, parent 2, commit 1.
# dir dir4: inode 6, parent 0, commit 1.
def test_phase_one_batch_three(cmd):
    cmd.add_u32(8)  # Nb change.

    # Delete file dir1/file4.txt.
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(4)  # Delete file.
    cmd.add_u64(5)  # Inode to delete.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Delete dir dir1/dir2.
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(5)  # Delete dir.
    cmd.add_u64(3)  # Inode to delete.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Create dir dir1/dir2.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(2)  # Create dir.
    cmd.add_u64(2)  # Parent inode.
    cmd.add_u64(1)  # Commit ID.
    cmd.add_str('dir2') # Entry path.
    
    # Fail delete file dir1/file2.txt (bad commit ID).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(4)  # Delete file.
    cmd.add_u64(4)  # Inode to delete.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail delete file dir4. (not a dir).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(4)  # Delete file.
    cmd.add_u64(6)  # Inode to delete.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail delete file ??? (bad inode).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(4)  # Delete file.
    cmd.add_u64(66) # Inode to delete.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail delete dir dir1 (directory not empty).
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(5)  # Delete dir.
    cmd.add_u64(2)  # Inode to delete.
    cmd.add_u64(1)  # Inode commit ID.
    
    # Fail delete root.
    cmd.add_u32(4)  # Compat.
    cmd.add_u32(5)  # Delete dir.
    cmd.add_u64(0)  # Inode to delete.
    cmd.add_u64(0)  # Inode commit ID.
    
# End result so far:
# file foo.txt: inode 1, parent 0, commit 1.
# dir dir1: inode 2, parent 0, commit 1.
# dir dir1/dir2: inode 7, parent 2, commit 3.
# file dir1/file2.txt: inode 4, parent 2, commit 2.
# dir dir4: inode 6, parent 0, commit 1.
def test_phase_one_batch_four(cmd):
    cmd.add_u32(13)  # Nb change.
    
    # Move file dir1/file2.txt to dir1/dir2/file2_moved.txt.
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(6)  # Move file.
    cmd.add_u64(4)  # Inode to move.
    cmd.add_u64(2)  # Inode commit ID.
    cmd.add_u64(7)  # Parent inode.
    cmd.add_u64(3)  # Parent commit ID.
    cmd.add_str("file2_moved.txt");
    
    # Move dir dir1 to dir4/dir1_moved.
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(2)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(6)  # Parent inode.
    cmd.add_u64(1)  # Parent commit ID.
    cmd.add_str("dir1_moved");
    
    # Create dir1.
    cmd.add_u32(5)  # Compat.
    cmd.add_u32(2)  # Create dir.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Commit ID.
    cmd.add_str('dir1') # Entry path.
    
    # Move dir4 to dir1/dir4_moved.
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(6)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Parent commit ID.
    cmd.add_str("dir1/dir4_moved");
    
    # Fail move file 'foo.txt' to dir1/dir4_moved/foo.txt (stale inode).
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(6)  # Move file.
    cmd.add_u64(1)  # Inode to move.
    cmd.add_u64(0)  # Inode commit ID.
    cmd.add_u64(6)  # Parent inode.
    cmd.add_u64(1)  # Parent commit ID.
    cmd.add_str("foo.txt");
    
    # Fail move file 'foo.txt' to dir1/dir4_moved/foo.txt (stale parent).
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(6)  # Move file.
    cmd.add_u64(1)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(6)  # Parent inode.
    cmd.add_u64(0)  # Parent commit ID.
    cmd.add_str("foo.txt");
    
    # Fail move file 'foo.txt' to dir1 (dest exist).
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(6)  # Move file.
    cmd.add_u64(1)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Parent commit ID.
    cmd.add_str("dir1");
    
    # Fail move file ??? to dir1/dir4_moved/foo.txt (inode does not exist).
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(6)  # Move file.
    cmd.add_u64(66) # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(6)  # Parent inode.
    cmd.add_u64(1)  # Parent commit ID.
    cmd.add_str("foo.txt");
    
    # Fail move dir dir1/dir4_moved/ to foo.txt/foo (component is not dir).
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(6)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(1)  # Parent inode.
    cmd.add_u64(1)  # Parent commit ID.
    cmd.add_str("foo");
    
    # Fail move dir foo.txt to foo2.txt (source is not dir).
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(1)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Parent commit ID.
    cmd.add_str("foo2.txt");
    
    # Fail move root.
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(0)  # Inode to move.
    cmd.add_u64(0)  # Inode commit ID.
    cmd.add_u64(0)  # Parent inode.
    cmd.add_u64(0)  # Parent commit ID.
    cmd.add_str("root");
    
    # Fail make directory dir1 child of itself.
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(8)  # Inode to move.
    cmd.add_u64(4)  # Inode commit ID.
    cmd.add_u64(8)  # Parent inode.
    cmd.add_u64(4)  # Parent commit ID.
    cmd.add_str("dirX");
    
    # Fail make directory dir4_moved child of itself.
    cmd.add_u32(7)  # Compat.
    cmd.add_u32(7)  # Move dir.
    cmd.add_u64(6)  # Inode to move.
    cmd.add_u64(1)  # Inode commit ID.
    cmd.add_u64(7)  # Parent inode.
    cmd.add_u64(3)  # Parent commit ID.
    cmd.add_str("dirX");
    
# End result:
# file foo.txt: inode 1, parent 0, commit 1.
# dir dir1: inode 8, parent 0, commit 4.
# dir dir1/dir4_moved: inode 6, parent 8, commit 1.
# dir dir1/dir4_moved/dir1_moved: inode 2, parent 6, commit 1.
# dir dir1/dir4_moved/dir1_moved/dir2: inode 7, parent 2, commit 3.
# file dir1/dir4_moved/dir1_moved/dir2/file2_moved.txt: inode 4, parent 7, commit 2.

def do_test_phase_two(db):
    
    # Prepare the phase 2 event.
    evt = ANP_msg()
    evt.add_bin("some phase two event")
    
    # Prepare the phase 2 argument.
    arg = ANP_msg()
    arg.add_u64(1)  # Workspace ID.
    arg.add_u32(0)  # User ID.
    arg.add_u32(0)  # Share ID.
    arg.add_u64(1)  # Commit ID.
    arg.add_bin(evt.get_payload()) # Event to post.
    
    # Execute the query.
    exec_query(db, "upload_phase_two", arg, phase_two_success)

def phase_two_success(ret):
    print "Success phase two."

def do_test_refresh(db):
    
    # Prepare the refresh argument.
    arg = ANP_msg()
    arg.add_u64(1)  # Workspace ID.
    arg.add_u32(0)  # User ID.
    arg.add_u32(0)  # Share ID.
    arg.add_u64(1)  # Commit ID.
    
    # Execute the query.
    exec_query(db, "refresh_upload", arg, refresh_success)

def refresh_success(ret):
    print "Success refresh."
    
def do_test_purge(db):
    
    # Prepare the purge argument.
    arg = ANP_msg()
    
    # Execute the query.
    exec_query(db, "purge_upload", arg, None)

def do_test_download(db):
    
    # Prepare the download argument.
    arg = ANP_msg()
    arg.add_u64(1)  # Workspace ID.
    arg.add_u32(0)  # Share ID.
    arg.add_u32(2)  # Number of downloads.
    arg.add_u64(1); # Inode.
    arg.add_u64(1); # Commit ID.
    arg.add_u64(4); # Inode.
    arg.add_u64(2); # Commit ID.
    
    # Execute the query.
    exec_query(db, "download_file", arg, download_success)

def download_success(ret):
    print "Success download."
    
    for i in range(0, 2):
        print "File %i: %s" % (i, ret.get_str(i + 1))

def main():

    # Open the connection.
    db = open_pg_conn("kas")
    
    # Add the first workspace.
    do_test_add_kws(db)
    
    # Do the phase one tests.
    if 1:
        do_test_phase_one(db, test_phase_one_batch_one)
        do_test_phase_one(db, test_phase_one_batch_two)
        do_test_phase_one(db, test_phase_one_batch_three)
        do_test_phase_one(db, test_phase_one_batch_four)
    
    # Test phase two.
    do_test_phase_two(db)
    
    # Test upload refresh.
    do_test_refresh(db)
    
    # Test upload purge.
    do_test_purge(db)
    
    # Test download.
    do_test_download(db)

main()

