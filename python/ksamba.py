#!/usr/bin/env python

SMB_UNIX = "SMB_UNIX"
SMB_WIN = "SMB_WIN"

class SmbParseException(Exception):
    pass

# This function extracts components from a string.
# input examples: 
# - //server/mount/
# - //server/mount/dir1/dir2/dir3
# - \\server\mount\dir1\dir2\dir3
# output examples:
# - server, mount, []
# - server, mount, [dir1, dir2]
# raise SmbParseException()
def samba_path_to_components(s):
    # First validation.
    if s.find("/") != -1 and s.find("\\") != -1: raise SmbParseException("Invalid path.")

    # Replace \ for / in string.
    s = s.replace("\\", "/")

    # Split string in an array.
    arr = s.split("/")
    if s.find("/") == -1:
        arr = [s]
   
    # Strip the last path separator if present.
    if arr[len(arr)-1] == "":
        arr = arr[:-1]

    # Validate.
    ok = True
    l = len(arr)
    if l < 4: ok = False
    elif  arr[0] != "" or arr[1] != "": ok = False
    elif "" in arr[2:]: ok = False
    if not ok: raise SmbParseException("Invalid path.")

    return arr[2], arr[3], arr[4:]

# This converts samba components to a path.
def samba_components_to_path(server, mount, dirs_list=None, mode=SMB_UNIX):
    sep = "/"
    if mode == SMB_WIN:
        sep = "\\"
        
    path = sep + sep + server + sep + mount + sep
    if dirs_list and len(dirs_list) > 0:
        path += sep.join(dirs_list) + sep
    
    return path

# Normalize path
def samba_normalize_path(s, mode=SMB_UNIX, include_dirs=True):
    server, mount, dirs_list = samba_path_to_components(s)
    if include_dirs:
        return samba_components_to_path(server, mount, dirs_list, mode)
    return samba_components_to_path(server, mount, None, mode)

# TESTS
def ksamba_test():
    list = [
            "aaaaa",
            "/fffdfd",
            "//dddddd",
            "//dddddd/",
            "//fdfdfd/fgfgdgfdgd",
            "//fdfdfd/f/",
            r"gdfgfdg\dfgfdgdfg",
            r"\fdgfgfdg\\",
            r"\dffdfd\sdfsf\fsdfsdf\sdfsdf",
            r"\\sdfsdf\fsfsf\gfdg\\dfg\dfgdf",
            r"\\aaaaadfsdf\fsfsf\gfdg\dfg\dfgdf"
            ]

    for el in list:
        try:
            server, mount, dirs_list = samba_path_to_components(el)
            
            print "Element='%s' server='%s' mount='%s' dirs='%s'." % ( el, server, mount, dirs_list )
            print "  UNIX mount path: '%s'" % ( samba_components_to_path(server, mount, None, SMB_UNIX) )
            print "  UNIX full path: '%s'" % ( samba_components_to_path(server, mount, dirs_list, SMB_UNIX) )
            print "  WIN full path: '%s'" % ( samba_components_to_path(server, mount, dirs_list, SMB_WIN) )

        except SmbParseException, e:
            print "Exception while parsing element '%s': '%s'" % ( el, str(e) )

if __name__ == "__main__":
    ksamba_test()

