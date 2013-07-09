#!/usr/bin/python

# This program monitors the state of the KCD and performs database cleanups.

from kfile import *
from krun import *
from kdaemonize import *
from klog import *
from kout import *
from kpg import *
from kanp import *
from kifconfig import *
from kasmodel import *
from kcd_client import BaseKcdClient
from kcd_lib import get_kcd_external_conf_object
from kfs_lib import KFS_FILE, KFSOpFileDelete 
import getopt

# Global configuration object.
admin_conf = None

# State of the monitor.
mon_state = None

# This class contains the global state of the program.
class MonState:
    def __init__(self):
        
        # Database purge interval.
        self.db_purge_interval = 0
    
        # Date at which we last tried to mount the samba share.
        self.smb_last_mount_time = 0
        
        # Date at which we last check if the samba share was working.
        self.smb_last_check_time = 0
        
        # True if the samba share is working properly.
        self.smb_up_flag = 0
        
        # Date at which we last purged the KFS shares.
        self.kfs_last_purge_time = 0
        
        # Date at which we last purged the attachments.
        self.att_last_purge_time = 0
        
        # Date at which we last purged the VNC sessions.
        self.vnc_last_purge_time = 0
        
        # True if an action has been done in the current round.
        self.action_flag = 0
        
        # Time to wait in select().
        self.time_to_wait = 0
        
        # Postgres DB connection.
        self.pg_conn = None
 
# This function parses the configuration file of this script and returns the
# corresponding configuration object.
def get_config_object():
    cfg = PropStore()
    
    root = RootConfigNode()
    root.load_master_config()
    key_list = [ "db_purge_interval", "kfs_mode", "kfs_dir", "smb_mount_unc", "smb_mount_point", "smb_mount_user", 
                 "smb_mount_pwd", "smb_mount_delay", "smb_check_delay", "smb_heartbeat_name" ]
    for name in key_list:
        cfg[name] = root["kcd_" + name]
    
    if not cfg.kfs_dir.endswith("/"): cfg.kfs_dir += "/"
    if not cfg.smb_mount_point.endswith("/"): cfg.smb_mount_point += "/"
    
    if cfg.kfs_mode == "samba" and cfg.smb_heartbeat_name == "":
        ip_addr = get_current_server_address()
        cfg.smb_heartbeat_name = ip_addr + ".heartbeat"
    
    return cfg

# Open the Postgres connection if required.
def open_pg_conn_if_needed():
    if mon_state.pg_conn == None:
        debug("Opening Postgres connection.")
        mon_state.pg_conn = open_pg_conn("kcd")
        
# This function returns true if the samba share is mounted.
def is_smb_share_mounted():
    
    f = open("/proc/mounts", "rb")
    lines = f.readlines()
    
    for line in lines:
        line = line.strip()
        fields = line.split()
        
        if len(fields) != 6:
            raise Exception("cannot parse /proc/mounts, invalid number of fields")
        
        mount_point = fields[1]
        if not mount_point.endswith("/"): mount_point += "/"
        
        if mount_point == admin_conf.smb_mount_point:
            return 1
    
    return 0

# This function unmounts the samba share if it is mounted.
def unmount_smb_share():
    if not is_smb_share_mounted(): return
    get_cmd_output(["/bin/umount", admin_conf.smb_mount_point])
    if is_smb_share_mounted(): raise Exception("failed to unmount samba share (unknown error)")

# This function mounts the samba share.
def mount_smb_share():

    # Unmount the samba share if required.
    unmount_smb_share()
    
    # Format the options.
    opt = "-o,user=" + admin_conf.smb_mount_user + ",pass=" + admin_conf.smb_mount_pwd +\
          ",iocharset=utf8"
    
    # Mount the share.
    get_cmd_output(["/sbin/mount.cifs", admin_conf.smb_mount_unc, admin_conf.smb_mount_point, opt])
    
    # Check if the share works.
    check_smb_share()

# This function checks if the samba share is functional. An exception is thrown
# on error.
def check_smb_share():
    
    # Make sure it is mounted.
    if not is_smb_share_mounted(): raise Exception("the samba share is not mounted")
    
    # Create the heartbeat file.
    hb_path = admin_conf.kfs_dir + admin_conf.smb_heartbeat_name
    now = str(int(time.time()))
    f = open(hb_path, "wb")
    f.write(now)
    f.close()
    
    # Re-read it.
    f = open(hb_path, "rb")
    val = f.read()
    f.close()
    
    # Make sure the values match.
    if now != val: raise Exception("heartbeat sanity check failed")

# This function unmounts the samba share if mounted.
def do_unmount():
    try:
        unmount_smb_share()
        return 0

    except Exception, e:
        out("Could not unmount samba share: %s." % str(e))

    return 1

# This function is called to monitor Samba.
def handle_samba():
    
    # The mode is not "samba". Just sleep.
    if admin_conf.kfs_mode != "samba":
        pass
    
    # The samba share is not mounted.
    elif not mon_state.smb_up_flag:
        
        # Compute the time elapsed since we last mounted the share.
        ttw = mon_state.smb_last_mount_time + admin_conf.smb_mount_delay - time.time()
        
        # Try to mount the share.
        if ttw <= 0:
            out("Attempting to mount samba share.") 
            mon_state.action_flag = 1
            
            # Try to mount.
            ok = 1
            try: mount_smb_share()
            except Exception, e:
                out("Mounting samba share failed: %s." % str(e))
                ok = 0
            
            # Reset the last mount time.
            mon_state.smb_last_mount_time = time.time()
            
            # If it worked, update our state.
            if ok:
                out("Mounting samba share successfully.")
                mon_state.smb_up_flag = 1
                mon_state.smb_last_check_time = time.time()
        
        # Wait a bit.
        else:
            mon_state.time_to_wait = min(ttw, mon_state.time_to_wait)
    
    # The samba share is mounted.
    else:
        # Compute the time elapsed since we last checked the share.
        ttw = mon_state.smb_last_check_time + admin_conf.smb_check_delay - time.time()
        
        # Check the share.
        if ttw <= 0:
            debug("Checking if samba share is working properly.")
            mon_state.action_flag = 1
            
            try:
                check_smb_share()
                mon_state.smb_last_check_time = time.time()
            
            except Exception, e:
                out("Detected samba share failure: %s." % str(e))
                mon_state.smb_up_flag = 0
        
        # Wait a bit.
        else:
            mon_state.time_to_wait = min(ttw, mon_state.time_to_wait)

# This function is called to monitor the KFS application.
def handle_kfs():

    # Check if we must purge the KFS share.
    ttw = mon_state.kfs_last_purge_time + admin_conf.db_purge_interval - time.time()

    if ttw <= 0:
        mon_state.action_flag = 1
        mon_state.kfs_last_purge_time = time.time()
    
        try:
            # Open the Postgres connection if required.
            open_pg_conn_if_needed()
            
            # Purge the shares.
            debug("Purging KFS shares.")
            arg = ANP_msg()
            exec_pg_query(mon_state.pg_conn, "SELECT purge_upload(%s)" % (escape_pg_bytea(arg.get_payload())))
            mon_state.pg_conn.commit()
        
        except Exception, e:
            out("Purging KFS shares failed: %s." % str(e))
            mon_state.pg_conn = None
        
    else:
        mon_state.time_to_wait = min(ttw, mon_state.time_to_wait)

# This function is called to handle the expired attachments.
def handle_att():
    ttw = mon_state.att_last_purge_time + admin_conf.db_purge_interval - time.time()
    
    if ttw <= 0:
        mon_state.action_flag = 1
        mon_state.att_last_purge_time = time.time()
        
        try:
            open_pg_conn_if_needed()
            
            debug("Purging attachments.")
            
            # Get the expired attachments.
            now = int(time.time())
            query = ("SELECT kws_id, email_id from kcd_kws_pub_email_info " +
                     "WHERE att_expire_flag = 0 AND att_expire_date < %i") % (now)
            cur = exec_pg_query(mon_state.pg_conn, query)
            kws_list = []
            for row in cur.fetchall(): kws_list.append((row[0], row[1]))
            mon_state.pg_conn.commit()
           
            # Purge the expired attachments.
            for kws_id, email_id in kws_list:
                
                # Get the associated files.
                query = ("SELECT inode_type, inode, commit_id FROM kcd_kws_kfs_current_view " +
                         "WHERE kws_id = %i AND share_id = 0 AND inode_type = %i AND email_id = %i") \
                          % (kws_id, KFS_FILE, email_id)
                cur = exec_pg_query(mon_state.pg_conn, query)
                file_list = []
                for row in cur.fetchall(): file_list.append((int(row[0]), long(row[1]), long(row[2])))
                mon_state.pg_conn.commit()
                
                # Connect to the KCD and delete the files.
                if len(file_list):

                    # Get a KCD client object.
                    kc = BaseKcdClient(get_kcd_external_conf_object())

                    # Build a list of entries.
                    delete_list = []
                    for inode_type, inode, commit_id in file_list:

                        assert inode_type == KFS_FILE
                        entry = KFSOpFileDelete(inode, commit_id)
                        delete_list.append(entry)

                    # Delete entries.
                    share_id = 0
                    user_id = 0
                    kc.kfs_delete_entries(kws_id, share_id, user_id, email_id, delete_list)

                # Mark the email as expired.
                arg = ANP_msg()
                arg.add_u64(kws_id)
                arg.add_u64(email_id)
                exec_pg_query(mon_state.pg_conn, "SELECT purge_att(%s)" % (escape_pg_bytea(arg.get_payload())))
                mon_state.pg_conn.commit()

            debug("Purged attachments.")
        
        except Exception, e:
            out("Purging attachments failed: %s." % str(e))
            mon_state.pg_conn = None
        
    else:
        mon_state.time_to_wait = min(ttw, mon_state.time_to_wait)

# This function is called to monitor the VNC application.
def handle_vnc():

    # Check if we must purge the VNC session.
    ttw = mon_state.vnc_last_purge_time + admin_conf.db_purge_interval - time.time()

    if ttw <= 0:
        mon_state.action_flag = 1
        mon_state.vnc_last_purge_time = time.time()
    
        try:
            # Purge the sessions.
            debug("Purging VNC sessions.")
            
            # Open the Postgres connection if required.
            open_pg_conn_if_needed()
            
            # Get the vncreflector port map.
            port_map = {}
            for line in get_cmd_output(["/bin/netstat", "-nltp"]).splitlines():
                fields = line.split()
                if len(fields) != 7 or not fields[0].startswith("tcp"): continue
                match = re.match(".*:(\d+)", fields[3])
                if not match: continue
                port = int(match.group(1))
                match = re.match(".*/(\w+)", fields[6])
                if not match: continue
                name = match.group(1)
                # Doesn't interact well with valgrind, skipping for now.
                #if name != "vncreflector": continue
                port_map[port] = 1
            
            # Determine which sessions are stale. A session is not stale if it
            # has not been started for at least 10 seconds. This gives the
            # process the opportunity to change its name to 'vncreflector'.
            stale_date = int(time.time()) - 10;
            cur = exec_pg_query(mon_state.pg_conn, "SELECT kws_id, session_id, port FROM kcd_kws_vnc_session " + 
                                                   "WHERE date <= " + str(stale_date))
            stale_list = []
            for row in cur.fetchall():
                port = row[2]
                if not port_map.has_key(port):
                    stale_list.append([row[0], row[1]])
            mon_state.pg_conn.commit()
            
            # Purge the stale sessions.
            for l in stale_list:
                (kws_id, session_id) = l
                debug("Purging session ID %i in workspace %i" % (session_id, kws_id))
                arg = ANP_msg()
                arg.add_u64(kws_id)
                arg.add_u32(0)
                arg.add_u64(session_id)
                arg.add_u32(2)
                arg.add_u32(0)
                arg.add_str("")
                exec_pg_query(mon_state.pg_conn, "SELECT end_vnc(%s)" % (escape_pg_bytea(arg.get_payload())))
                mon_state.pg_conn.commit()
            
        except Exception, e:
            out("Purging VNC sessions failed: %s." % str(e))
            mon_state.pg_conn = None
        
    else:
        mon_state.time_to_wait = min(ttw, mon_state.time_to_wait)

# This function contains the main loop of the monitor.
def main_loop():
    global mon_state
    
    # Check if the share is already mounted.
    if admin_conf.kfs_mode == "samba":
        debug("Checking if samba share is already mounted.")
        mon_state.smb_up_flag = is_smb_share_mounted()
     
    while 1:
        
        # Reset the action and time_to_wait fields.
        mon_state.action_flag = 0
        mon_state.time_to_wait = 1000
        
        # Dispatch.
        handle_samba()
        handle_kfs()
        handle_vnc()
        handle_att()
        
        # Wait for something to happen.
        if not mon_state.action_flag:
	    select_wrapper([], [], [], mon_state.time_to_wait + 0.01)

def usage():
    s = "Usage: " + sys.argv[0] + " [options] \n" +\
        " Options:\n" +\
        " -h, --help:                             print help and exit.\n" +\
        " -u, --unmount:                          umount samba filesystem if mounted\n" +\
        " -d, --debug:                            prints debug information\n" +\
        " -t, --daemonize:                        daemonize\n" +\
        " -p <pidfile>, --pidfile <pidfile>:      write pid to file\n" +\
	"\n"
    out(s)

def main():
    global mon_state
    global admin_conf
    
    ret_code = 0
    
    try:
	opts, args = getopt.getopt(sys.argv[1:], "hudtp:", ["help", "unmount", "debug", "daemonize", "pidfile="])
    except getopt.GetoptError, e:
	sys.stderr.write("Options error: '%s'\n" % (str(e)) )
	usage()
	os._exit(1)
    
    help_flag = 0
    unmount_flag = 0
    detach_flag = 0
    debug_flag = 0

    pidfile = None
    
    for k, v in opts:
	if k == "-h" or k == "--help":
            help_flag = 1
        elif k == "-u" or k == "--unmount":
            unmount_flag = 1
	elif k == "-d" or k == "--debug":
	    debug_flag = 1
	elif k == "-t" or k == "--daemonize":
	    detach_flag = 1
	elif k == "-p" or k == "--pidfile":
	    pidfile = v
	    
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

    # Initialize the monitor state.
    mon_state = MonState()
    
    # Unmount samba share if mounted.
    if unmount_flag:
        ret_code = do_unmount()

    else:
        # Daemonize.
        if detach_flag:
            # Redirect out, err and debug to syslog.
            do_logredirect()

            # Daemonize.
            daemonize()

        # Write PID file if asked to do so.
        if pidfile:
            pid = os.getpid()
            debug("Writing pid to '%s'" % (pidfile))
            try: write_file(pidfile, str(pid))
            except Exception, e:
                err("Could not write pid file '%s': %s" % (pidfile, str(e)) )
                err("Exiting.")
                os._exit(1)

        # Run monitor until sun dies.
        try: main_loop()
        except Exception, e:
            err("Monitoring process failed: %s." % (str(e)))
            ret_code = 1
        
        # Delete the PID if required.
        if pidfile: delete_file(pidfile)
        
    # Exit.
    os._exit(ret_code)

main()

