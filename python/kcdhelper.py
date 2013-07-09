#!/usr/bin/python

# This program performs miscellaneous jobs on the KCD.

from kfile import *
from krun import *
from klog import *
from kout import *
from kpg import *
from kanp import *
import ConfigParser, getopt, shutil

# Path to the kfs.ini file.
kfs_ini_path="/etc/teambox/kcd/kfs.ini"

# Global configuration object.
admin_conf = None

# Postgres DB connection.
db = None
 
# This function parses the configuration file of this script and returns the
# corresponding configuration object.
def get_config_object():
    cfg = PropStore()
    parser = read_ini_file(kfs_ini_path)
    cfg.kfs_dir = parser.get("config", "kfs_dir")
    if not cfg.kfs_dir.endswith("/"): cfg.kfs_dir += "/"
    return cfg
        
def usage():
    s = "Usage: " + sys.argv[0] + " [options] \n" +\
        " Options:\n" +\
        " -h, --help:                             print help and exit.\n" +\
        " -d, --debug:                            prints debug information\n" +\
        " --delete-kws <kws_id>:                  delete the workspace specified.\n" +\
        " --sync-kfs <kws_id>:                    synchronize the KFS files specified.\n"
    out(s)

# Execute the query specified and close the cursor obtained.
def write_query(query):
    exec_pg_query(db, query).close()

# Return the status of the workspace specified: "none", "deleted", "live".
def get_kws_status(kws_id):
    cur = exec_pg_query(db, "SELECT flags FROM kcd_kws_list WHERE kws_id = %i" % (kws_id))
    row = cur.fetchone()
    if row == None: status = "none"
    elif row[0] & KANP_KWS_FLAG_DELETE: status = "deleted"
    else: status = "live"
    cur.close()
    return status

# Delete the workspace specified. First we make the change in the database, then
# we synchronize the workspace files.
def delete_kws(kws_id):
    arg = ANP_msg()
    arg.add_u64(kws_id)
    write_query("SELECT delete_kws(E'%s')" % (pgdb.escape_bytea(arg.get_payload())))
    db.commit()
    sync_kfs(kws_id)

# Synchronize the files on the KFS. If the workspace is deleted, the workspace
# KFS directory is deleted. Otherwise, the files in the KFS directory that are
# not present in the KFS file map are deleted.
def sync_kfs(kws_id):
    status = get_kws_status(kws_id)
    kws_path = "%s%i/" % (admin_conf.kfs_dir, kws_id)
    
    if status != "live":
        shutil.rmtree(kws_path, 1)
    
    else:
        path_dict = {}
        cur = exec_pg_query(db, "SELECT path FROM kcd_kws_kfs_file_map WHERE kws_id = %i AND share_id = 0" % (kws_id))
        for row in cur.fetchall():
            path_dict[row[0]] = 1
        cur.close()
        
        for root, dirs, files in os.walk(kws_path):
            if not root.startswith(kws_path): raise Exception("walk error")
            for file in files:
                full_path = os.path.join(root, file)
                share_path = full_path[len(kws_path):]
                if not path_dict.has_key(share_path):
                    try: delete_file(full_path)
                    except: pass
    
def main():
    global admin_conf, db
    
    ret_code = 0
    
    try:
	opts, args = getopt.getopt(sys.argv[1:], "hd", ["help", "debug", "delete-kws=", "sync-kfs="])
    except getopt.GetoptError, e:
	sys.stderr.write("Options error: '%s'\n" % (str(e)) )
	usage()
	os._exit(1)
    
    help_flag = 0
    debug_flag = 0
    delete_kws_flag = 0
    sync_kfs_flag = 0
    kws_id = 0
    
    for k, v in opts:
	if k == "-h" or k == "--help":
            help_flag = 1
	elif k == "-d" or k == "--debug":
	    debug_flag = 1
	elif k == "--delete-kws":
	    delete_kws_flag = 1
            kws_id = int(v)
	elif k == "--sync-kfs":
            sync_kfs_flag = 1
            kws_id = int(v)
	    
    if len(args):
	usage()
	os._exit(1)
    
    # Print help.
    if help_flag:
        usage()
        os._exit(0)

    # Enable debug.
    if debug_flag: do_debug()
    
    # Parse the configuration.
    admin_conf = get_config_object()
        
    # Open the Postgres connection.
    debug("Opening Postgres connection.")
    db = open_pg_conn("kcd")
    
    # Dispatch.
    try:
        if delete_kws_flag: delete_kws(kws_id)
        elif sync_kfs_flag: sync_kfs(kws_id)
    except Exception, e:
        out("Error: %s"  % (str(e)))
        ret_code = 1

    # Exit.
    os._exit(ret_code)

main()

