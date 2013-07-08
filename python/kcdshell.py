#!/usr/bin/env python

import getopt, syslog, ConfigParser, os
from pyPgSQL import PgSQL

# from kpython
from kprompt import *
from kpg import *
import kdebug

# from kas-python
from kcd_lib import get_kcd_external_conf_object, WorkspaceInvitee
from kcd_client import BaseKcdClient
import kanp
import kfs_lib

# Platform shell class.
class KcdShell:
    def __init__(self):
    
        # Default KCD settings.
        self.kcd_host = "localhost"
        self.kcd_port = 443
        
        # Default KCD DB settings.
        self.db_host = None
        self.db_port = 5432
        self.db_database = "kcd"
        self.db_user = ""
        self.db_pass = ""

        # Default KFS variables.
        self.user_id = 1
        self.parent_inode = 0

        # Standard output stream. Only the 'write' method is supported.
        self.stdout = sys.stdout
        
        # Standard error stream. Only the 'write' method is supported.
        self.stderr = sys.stderr
        
        # True if the commands run must be echo'ed.
        self.echo_cmd_flag = 0
        
        # Trapped exception list.
        self.trapped_exception_list = (KeyboardInterrupt, EOFError, SystemExit, Exception)
       
        # Read configuration file.
        self.cfg = self.read_config()

        # Help strings.
        self.global_help_str = \
            "KCD shell.\n" +\
            "\n" +\
            "Commands:\n" +\
            "  help         Show help about a command.\n" +\
            "  upload       Upload a file to the KCD.\n" +\
            "  download     Download a file from the KCD.\n" +\
            "  rm           Delete a file from the KCD.\n" +\
            "  reinvite     Reinvite a user in all his workspaces.\n" +\
            "\n" +\
            "Global options:\n" +\
            "  -h, --help [cmd]        Print help and exit.\n" +\
            "  -s, --syslog            Log output to syslog.\n" +\
            "  -e, --echo              Echo the name of every command run.\n"

        self.help_help_str = \
            "help [command]\n" +\
            "\n" +\
            "Show help about a command, or list the commands supported.\n"

        self._help_str = \
            "services\n" +\
            "\n" +\
            "Show the status of the services and their associated addresses.\n"
            
        self.upload_help_str = \
            "upload <kws_id> <file>\n" +\
            "\n" +\
            "Upload a file to KCD.\n" +\
            "\n" +\
            "  -d, --debug [module:level]     Set debug level for module. Can use several times.\n" +\
            "  -s, --share_id                 Share id. Default is 0.\n" +\
            "  -u, --user_id                  User ID. Default is 1.\n" +\
            "  -a, --upload_file_as           Upload file as...\n"   

        self.download_help_str = \
            "download <kws_id> <commit_id>\n" +\
            "\n" +\
            "Download a file from KCD.\n" +\
            "\n" +\
            "  -d, --debug [module:level]     Set debug level for module. Can use several times.\n" +\
            "  -s, --share_id                 Share id. Default is 0.\n" +\
            "  -u, --user_id                  User ID. Default is 1.\n" +\
            "  -a, --download_file_as         Download file as...\n"   

        self.rm_help_str = \
            "rm <kws_id> <inode_id> <commit_id>\n" +\
            "\n" +\
            "  -d, --directory                Entry is a directory. Default is file.\n" +\
            "  -s, --share_id                 Share id. Default is 0.\n" +\
            "  -u, --user_id                  User ID. Default is 1.\n" +\
            "  -e, --email_id                 Email ID. Default is <None>.\n"

        self.reinvite_help_str = \
            "reinvite <email>\n" +\
            "\n" +\
            "Reinvite a user in all the workspaces in which he is present on the KCD. The\n" +\
            "user is not reinvited if he is banned.\n" +\
            "\n" +\
            "  -d, --dry-run                  Just show what would happen.\n"
            
        # Command dispatch table. The first column is the command name, the
        # second is the number of arguments, the third is the short options
        # accepted, the fourth is the long options accepted, the fifth is the
        # handler function to call, the sixth is the help string. 'None' can be
        # specified for the number of arguments when the command takes a
        # variable number of arguments. The arguments supplied to the handler
        # are the values returned by getopt().
        self.cmd_dispatch_table = \
            (
                ("help", None, "", [], self.handle_help, self.help_help_str),
                
                # CHECK CODE BEFORE ENABLING THOSE
                #("upload", 2, "d:s:u:a:", ["debug=", "share_id=", "user_id=", "upload_file_as="],
                #    self.handle_upload, self.upload_help_str),
                #("download", 2, "d:s:u:a:", ["debug=", "share_id=", "user_id=", "download_file_as="],
                #    self.handle_download, self.download_help_str),

                ("rm", 3, "d:t:s:u:e:", ["debug=", "type=", "share_id=", "user_id=", "email_id="],
                  self.handle_rm, self.rm_help_str),
                
                ("reinvite", 1, "d", ["dry-run"], self.handle_reinvite, self.reinvite_help_str)
            )

    # Read config file(s).
    def read_config(self):
        parser = ConfigParser.ConfigParser()
        parser.read([os.path.expanduser('~/.kcdclient.ini'), '/etc/teambox/kas-python/kcdshell.ini'])

        # KCD options.
        try:  self.kcd_host = parser.get("config", "kcd_host")
        except Exception: pass
        try: self.kcd_port = int(parser.get("config", "kcd_port"))
        except Exception: pass

        # Database options.
        try: self.db_host = parser.get("config", "db_host")
        except Exception: pass
        try: self.db_port = int(parser.get("config", "db_port"))
        except Exception: pass
        try: self.db_user = parser.get("config", "db_user")
        except Exception: pass
        try: self.db_pass = parser.get("config", "db_pass")
        except Exception: pass
        try: self.db_database = parser.get("config", "db_database")
        except Exception: pass
 
    # Get a database connenction.
    def get_db_conn(self):
        kdebug.debug(2, "Connecting to %s:%i, database %s, user %s, pass %s" % \
            ( self.db_host, self.db_port, self.db_database, self.db_user, self.db_pass ) )
        conn = PgSQL.connect(host=self.db_host, port=self.db_port, database=self.db_database,
            user=self.db_user, password=self.db_pass)
        kdebug.debug(1, "Connecting to %s:%i, database %s, user %s, pass %s" % \
            ( self.db_host, self.db_port, self.db_database, self.db_user, self.db_pass ) )
        return conn

    # Print the program usage.
    def print_usage(self, stream):
        stream.write(self.global_help_str)

    # Enable logging to syslog.
    def enable_syslog(self):
        class SyslogStream:
            def write(self, msg):
                syslog.syslog(syslog.LOG_INFO, msg)
        
        syslog.openlog(os.path.basename(sys.argv[0]), 0, syslog.LOG_DAEMON)
        stream = SyslogStream()
        self.stdout = stream
        self.stderr = stream
    
    # Return the list of commands matching the name specified.
    def get_cmd_list_from_name(self, name):
        l = []
        for entry in self.cmd_dispatch_table:
            if entry[0].startswith(name): l.append(entry)
        return l

    # Setup readline.
    def setup_readline(self):
        cmd_name_list = []
        for entry in self.cmd_dispatch_table: cmd_name_list.append(entry[0])
        completer = readline_completer(cmd_name_list)
        readline.parse_and_bind("tab: complete")
        readline.set_completer(completer.complete)

    # Command handlers.
    def handle_help(self, opts, args):
        
        # Print help about the command specified, if there is one.
        if len(args):
            l = self.get_cmd_list_from_name(args[0])
            if len(l) == 0:
                self.stdout.write("No such command.\n")
                self.print_usage(self.stdout)
            else:
                first = 1
                for cmd in l:
                    if not first: self.stdout.write("\n")
                    self.stdout.write(cmd[5])
                    first = 0
        
        # Print global help.
        else:
            self.print_usage(self.stdout)

    # Handle kdebug options
    def set_debug_levels(self, opts):
        for opt, value in opts:
            if opt == '-d' or opt == '--debug':
                debug_id, level = value.split(":")
                kdebug.set_debug_level(level, debug_id)
 
    # Upload a file to the KCD.
    def handle_upload(self, opts, args):
        # KDebug parameters
        self.set_debug_levels(opts)

        # Arguments
        self.kws_id = int(args[0])
        self.file = args[1]
        
        # Options
        self.user_id = 1
        self.share_id = 0
        self.upload_file_as = os.path.basename(self.file)
        for opt, value in opts:
            if opt == "-u" or opt == "--user_id":
                self.user_id = value
            if opt == "-s" or opt == "--share_id":
                self.share_id = value
            if opt == "-a" or opt == "--upload_file_as":
                self.upload_file_as = value
 
        # Connect to the KCD database.
        self.conn = self.get_db_conn()

        # Connect to KCD
        kdebug.debug(2, "Instantiating a KCD client.")
        c = BaseKcdClient(get_kcd_external_conf_object(), db_conn=self.conn)
        kdebug.debug(1, "Instantiated a KCD client.")

        # Prepare file list.       
        files = []
        file = kfs_lib.KFSUploadFile()
        file.kfs_op = kanp.KANP_KFS_OP_CREATE_FILE
        file.parent_inode = 0
        file.parent_commit_id = 0
        file.name = self.upload_file_as
        fd = os.open(self.file, os.O_RDONLY)
        file.set_from_fd(fd)
        files.append(file)

        # Connecting to KCD.
        kdebug.debug(2, "Connecting to KCD.")
        c.connect()
        kdebug.debug(1, "Connected to KCD.")

        # Selecting KFS role.
        kdebug.debug(2, "Selecting KFS role.")
        c.select_role(kanp.KANP_KCD_ROLE_FILE_XFER)
        kdebug.debug(1, "Selected KFS role.")

        # Uploading file(s).
        kdebug.debug(2, "Uploading file(s) to kcd %i, share %i, user %i." % \
            ( self.kws_id, self.share_id, self.user_id ) )
        c.kfs_upload(self.kws_id, self.share_id, self.user_id, files)
        kdebug.debug(1, "Uploaded file(s) to kcd %i, share %i, user %i." % \
            ( self.kws_id, self.share_id, self.user_id ) )

        # Errors?
        i = -1
        for file in files:
            i += 1
            if file.kfs_error:
                print "File %i could not be uploaded: '%s'." % ( i, file.kfs_error )

        # Close  KCD connection.
        c.close()

    # Download a file from the KCD.
    def handle_download(self, opts, args):
        # KDebug parameters
        self.set_debug_levels(opts)

        # Arguments
        self.kws_id = int(args[0])
        self.file = args[1]
        
        # Options
        self.user_id = 1
        self.share_id = 0
        self.download_file_as = self.file
        for opt, value in opts:
            if opt == "-u" or opt == "--user_id":
                self.user_id = value
            if opt == "-s" or opt == "--share_id":
                self.share_id = value
            if opt == "-a" or opt == "--download_file_as":
                self.download_file_as = value
            if opt == "-p" or opt == "--parent_id":
                self.parent_id = value
 
        # Connect to the KCD database.
        self.conn = self.get_db_conn()

        # Connect to KCD
        kdebug.debug(2, "Instantiating a KCD client.")
        c = BaseKcdClient(get_kcd_external_conf_object(), db_conn=self.conn)
        kdebug.debug(1, "Instantiated a KCD client.")

        # Get the entry details.
        entry = kfs_lib.kfs_kas_view_lookup_file(self.conn, self.kws_id, self.share_id, self.file, self.parent_inode)
        if not isinstance(entry, kfs_lib.KFSFile): raise Exception("This entry is not a file.")

        # Prepare file list.
        files = []
        file = kfs_lib.KFSDownloadFile()
        file.inode = entry.inode
        file.commit_id = entry.commit_id
        file.size = entry.size
        file.comm = kfs_lib.KFSFileWriter(self.download_file_as)
        files.append(file)

        # Connecting to KCD.
        kdebug.debug(2, "Connecting to KCD.")
        c.connect()
        kdebug.debug(1, "Connected to KCD.")

        # Selecting KFS role.
        kdebug.debug(2, "Selecting KFS role.")
        c.select_role(kanp.KANP_KCD_ROLE_FILE_XFER)
        kdebug.debug(1, "Selected KFS role.")

        # Uploading file(s).
        kdebug.debug(2, "Downloading file(s) from kcd: kws_id %i, share %i, user %i." % \
            ( self.kws_id, self.share_id, self.user_id ) )
        c.kfs_download(self.kws_id, self.share_id, self.user_id, files)
        kdebug.debug(1, "Downloaded file(s) from kcd: kws_id %i, share %i, user %i." % \
            ( self.kws_id, self.share_id, self.user_id ) )

        # Errors?
        i = -1
        for file in files:
            i += 1
            if file.kfs_error:
                print "File %i could not be downloaded: '%s'." % ( i, file.kfs_error )

        # Close  KCD connection.
        c.close()

    # Delete a file from KCD.
    def handle_rm(self, opts, args):
        # KDebug parameters
        self.set_debug_levels(opts)

        # Arguments
        self.kws_id = int(args[0])
        self.inode_id = long(args[1])
        self.commit_id = long(args[2])
        
        # Options
    	self.type = 'f'
        self.user_id = 1
        self.share_id = 0
        self.email_id = 0
        for opt, value in opts:
            if opt == "-t" or opt == "--type":
                self.type = value
            if opt == "-u" or opt == "--user_id":
                self.user_id = value
            if opt == "-s" or opt == "--share_id":
                self.share_id = value
            if opt == "-e" or opt == "--email_id":
                self.email_id = value
 
        # Connect to KCD
        kdebug.debug(2, "Instantiating a KCD client.")
        c = BaseKcdClient(get_kcd_external_conf_object())
        kdebug.debug(1, "Instantiated a KCD client.")

        # Build list of entries (only one entry supported for now).
        entries_list = []
        if self.type == 'd':
            entry = kfs_lib.KFSOpDirDelete(self.inode_id, self.commit_id)
        elif self.type == 'f':
            entry = kfs_lib.KFSOpFileDelete(self.inode_id, self.commit_id)
        else:
            raise Exception("Bad entry type.")
        entries_list = [entry]

        # Delete entries.
        c.kfs_delete_entries(self.kws_id, self.share_id, self.user_id, self.email_id, entries_list)  

    # Reinvite a user in all his workspaces.
    def handle_reinvite(self, opts, args):
        # Arguments
        email = args[0]
        
        # Options
        dry_run_flag = 0
        for opt, value in opts:
            if opt == "-d" or opt == "--dry-run": dry_run_flag = 1
 
        # Connect to the KCD database.
        self.conn = self.get_db_conn()
        
        # Connect to KCD
        kdebug.debug(2, "Instantiating a KCD client.")
        c = BaseKcdClient(get_kcd_external_conf_object())
        c.connect()
        c.select_role(kanp.KANP_KCD_ROLE_WORKSPACE)
        kdebug.debug(1, "Instantiated a KCD client.")
        
        # Get the list of workspaces.
        kws_list = []
        query = ("SELECT kws_id FROM kcd_kws_users WHERE lower(email)=lower(%s) AND (bool(flags & %i = 0)) "
                 "ORDER BY kws_id") % (escape_pg_string(email), kanp.KANP_USER_FLAG_BAN)
        cur = exec_pg_query(self.conn, query)
        for row in cur.fetchall(): kws_list.append(row[0])
        cur.close()
        
        # Invite the user.
        for kws_id in kws_list:
            self.stdout.write("Reinviting user %s to workspace %i.\n" % (email, kws_id))
            if dry_run_flag: continue
            
            # Log in the workspace.
            c.connect_workspace(workspace_id=kws_id, email_id="kwmo", password=c.conf.db_passwd)
            
            # Send the invitation.
            msg = "This email was sent automatically to give you access to your Teambox.\n"
            wi = WorkspaceInvitee(email_address=email, send_mail=1)
            c.send_invitation(kws_id, msg, [ wi ])
            if wi.error != "": raise Exception(wi.error)

    # Run the specified command. This method must be passed a list containing
    # the command name and its arguments. The method returns 0 on success, 1 on
    # failure.
    def run_command(self, input_arg_list):
        if self.echo_cmd_flag:
            s = "Running command: " 
            for arg in input_arg_list: s += arg + " "
            s += "\n"
            self.stdout.write(s)
        
        cmd_list = self.get_cmd_list_from_name(input_arg_list[0])
        if len(cmd_list) != 1:
            if len(cmd_list) == 0:
                self.stderr.write("No such command.\n")
            else: 
                self.stderr.write("Ambiguous command: ")
                for cmd in cmd_list: self.stderr.write(cmd[0] + " ")
                self.stderr.write("\n")
            return 1
            
        cmd = cmd_list[0]
        
        # Parse the options of the command.
        try: cmd_opts, cmd_args = getopt.getopt(input_arg_list[1:], cmd[2], cmd[3])
        except getopt.GetoptError, e:
            self.stderr.write("Command options error: %s.\n\n" % (str(e)))
            self.stderr.write(cmd[5])
            return 1
       
        # Verify the number of arguments.
        if cmd[1] != None and cmd[1] != len(cmd_args):
            self.stderr.write("Invalid number of arguments.\n\n")
            self.stderr.write(cmd[5])
            return 1
        
        # Call the handler.
        cmd[4](cmd_opts, cmd_args)

    # This method implements a high-level exception handler.
    def high_level_exception_handler(self, e, ignore_error=0):
        
        # Raise system exit exceptions.
        if isinstance(e, SystemExit): raise e
        
        # Ignore interruptions.
        elif isinstance(e, KeyboardInterrupt) or isinstance(e, EOFError): return
         
        # Print errors, exit if requested.
        else:
            self.stderr.write("Error: " + str(e) + ".\n")
            if ignore_error: return
            sys.exit(1)

    # Loop processing commands.
    def handle_shell_mode(self):
        
        # Setup readline.
        self.setup_readline()
        
        # Print greeting.
        s = "Teambox platform shell.\n" +\
            "Type 'help' for help.\n" +\
            "\n"
        self.stdout.write(s)
        
        while 1:
            # Wait for command.
            cmd_line = string.split(raw_input("> "))
            if len(cmd_line) == 0: continue
            
            # Run the command.
            try: self.run_command(cmd_line)
            
            # Ignore interrupts.
            except self.trapped_exception_list, e: self.high_level_exception_handler(e, 1)
            
            # Add an empty line so that it looks pretty.
            finally: print ""

def main():
    help_flag = 0
    syslog_flag = 0
    
    # Create an instance of the shell.
    shell = KcdShell()
    
    # Parse the global options.
    try: opts, args = getopt.getopt(sys.argv[1:], "hse", ["help", "syslog", "echo"])
    except getopt.GetoptError, e:
        sys.stderr.write("Options error: %s.\n\n" % (str(e)))
        shell.print_usage(sys.stderr)
        sys.exit(1)
    
    for k, v in opts:
        if k == "-h" or k == "--help": help_flag = 1
        elif k == "-s" or k == "--syslog": syslog_flag = 1
        elif k == "-e" or k == "--echo": shell.echo_cmd_flag = 1
    
    # Handle help.
    if help_flag:
        shell.handle_help({}, args)
        sys.exit(0)
    
    # Setup syslog.
    if syslog_flag: shell.enable_syslog()
    
    # Get the invokation name.
    invoked_name = os.path.basename(sys.argv[0])
    
    if 1:
    #try:
        # Handle shell_mode.
        if invoked_name == "kcdshell" and not len(args):
            shell.handle_shell_mode()
        
        # Run the specified command.
        else:
            sys.exit(shell.run_command(args))
    
    # Handle the exceptions.
    #except shell.trapped_exception_list, e: shell.high_level_exception_handler(e, 0)
    
    # Wait before exiting if necessary.
    #finally:
    #    pass
 
# Allow import of this module.
if __name__ == "__main__":
    main()

