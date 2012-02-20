from kwmo.model.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *
from sqlalchemy.sql import and_
from sqlalchemy.databases.postgres import PGBigInteger
from sqlalchemy import Table, Column, ForeignKeyConstraint

# from local project
from kwmo.model.workspace import Workspace
from kfs_lib import KFS_DIR, KFS_FILE, KFS_STATUS_OK, KFS_STATUS_DELETED
from kanp import isUInt32, isUInt64

import logging
log = logging.getLogger(__name__)

class KfsNode(Entity):
    using_options(shortnames=True)

    inode_id = Field(PGBigInteger, primary_key=True, autoincrement=False)
    share_id = Field(Integer, primary_key=True, autoincrement=False)
    workspace = ManyToOne("Workspace", primary_key=True)
    user_id = Field(Integer) # No relation needed
    commit_id = Field(PGBigInteger)
    inode_type = Field(Integer)
    name = Field(Text)
    status = Field(Integer)
    cdate = Field(PGBigInteger)
    mdate = Field(PGBigInteger)
    file_size = Field(PGBigInteger)
    file_hash = Field(Binary)

    parent = ManyToOne("KfsNode")
    children = OneToMany("KfsNode")

    # Check if self is a directory.
    def isDir(self):
        return (self.inode_type == KFS_DIR)
    
    # Check if self is a directory.
    def isFile(self):
        return (self.inode_type == KFS_FILE)

    def getSubdirs(self):
        assert self.isDir()
        return self.query.filter_by(parent = self, inode_type=KFS_DIR, status=KFS_STATUS_OK)
    
    def getFiles(self):
        assert self.isDir()
        return self.query.filter_by(parent = self, inode_type=KFS_FILE)

    def getOKFiles(self):
        assert self.isDir()
        return self.query.filter_by(parent = self, inode_type=KFS_FILE, status=KFS_STATUS_OK)

    def getAllButDeletedFiles(self):
        assert self.isDir()
        return self.query.filter(and_(KfsNode.parent == self, KfsNode.inode_type == KFS_FILE, KfsNode.status != KFS_STATUS_DELETED))
        
    def getSubdirsCount(self):
        #This doesn't return a resultset then counts it, instead sqlalchemy is smart enough to issue a count(1) statment
        return self.getSubdirs().count()
    
    def getFilesCount(self):
        #This doesn't return a resultset then counts it, instead sqlalchemy is smart enough to issue a count(1) statment
        return self.getFiles().count()
        
    def getOKFilesCount(self):
        #This doesn't return a resultset then counts it, instead sqlalchemy is smart enough to issue a count(1) statment
        return self.getOKFiles().count()
  
    # Delete files that have a deleted status.
    def deleteDeletedChilds(self):
        deleted_childs = self.query.filter_by(parent = self, inode_type=KFS_FILE, status=KFS_STATUS_DELETED)
        for deleted_child in deleted_childs:
            deleted_child.delete()
     
    # Returns a list of parents (grand-parents before parents)
    def getParents(self):
        plist = []
        node = self
        while 1:
            parent = node.parent
            if not parent: break
            plist.append(parent)
            node = parent
        plist.reverse()
        return plist

    # Returns the parent node or None
    def getParent(self):
        return node.parent

    # Get the path components (ignore the root directory).
    def getPathComponents(self):
        dir_list = self.getParents()
        if self.isDir(): dir_list += [self]
        components = []
        for dir in dir_list:
            components.append(dir)
        if self.isFile(): components += [self]
        return components

def get_root_dir(workspace_id, share_id):
    inode_id = 0
    node = KfsNode.get_by(workspace_id=workspace_id, share_id=share_id, inode_id=inode_id)
    assert node.isDir()
    return node

def get_dir(workspace_id, share_id, inode_id):
    node = KfsNode.get_by(workspace_id=workspace_id, share_id=share_id, inode_id=inode_id)
    assert node.isDir()
    return node

