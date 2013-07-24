# from system
import ConfigParser, random, struct
import psycopg2 as PgSQL

from kpg import open_pg_conn
from kasmodel import RootConfigNode

# from kpython
import kbase
from kbase import PropStore

# Put after imports so log is not overwridden by an imported module.
#log = logging.getLogger(__name__)

# Default master file path
default_master_file_path = RootConfigNode.master_file_path

# This function generates a 16 bytes long (pseudo) random string.
def get_kcd_ticket_rnd():
    s = ""
    for i in range(0,16):
        i = random.randint(0, 255)
        s += struct.pack(">c", chr(i))

    return s

# Get master config.
def get_master_config(path=default_master_file_path):
    master_config = RootConfigNode()
    master_config.load_master_config(path=path)
    return master_config

# Get local kcd config object.
def get_local_kcd_conf_object(master_config=None, path=default_master_file_path):
    # Get master configuration, if not provided.
    if not master_config:
        master_config = get_master_config(path=path)

    # Initialize object.
    conf = PropStore()

    # Fill db options (connect through local socket).
    conf.db_host = ''
    conf.db_port = 0
    conf.db_user = ''
    conf.db_passwd = ''

    # Fill KCD options.
    conf.kcd_host = master_config.kcd_host
    conf.kcd_port = master_config.kcd_listen_port
    conf.kcd_passwd = master_config.kcd_pwd
    if conf.kcd_passwd == None or conf.kcd_passwd == '':
        conf.kcd_passwd = master_config.admin_pwd

    return conf

# Get kcd external config object.
def get_kcd_external_conf_object(master_config=None, path=default_master_file_path):
    # Get master configuration, if not provided.
    if not master_config:
        master_config = get_master_config(path=path)

    # Initialize object.
    conf = PropStore()

    # Fill db options.
    conf.db_host = master_config.kcd_db_host
    conf.db_port = master_config.kcd_db_port
    conf.db_user = master_config.kcd_db_user
    conf.db_passwd = master_config.kcd_db_pwd

    # Fill KCD options.
    conf.kcd_host = master_config.kcd_host
    conf.kcd_port = master_config.kcd_listen_port
    conf.kcd_passwd = master_config.kcd_pwd
    if conf.kcd_passwd == None or conf.kcd_passwd == '':
        conf.kcd_passwd = master_config.admin_pwd

    return conf

# This function returns a connection to the KCD database.
def get_kcd_db_conn(conf):
    database = 'kcd'
    if conf.db_host == '':
        return open_pg_conn(database)
    else:
        return PgSQL.connect(host=conf.db_host, port=conf.db_port, database=database,
                         user=conf.db_user, password=conf.db_passwd)

# This class represents an KCD invitee.
class WorkspaceInvitee:
    def __init__(self, real_name='', email_address='', key_id=0, org_name='', password='', send_mail=False):
        self.real_name = real_name
        self.email_address = email_address
        self.key_id = key_id
        self.org_name = org_name
        self.password = password
        self.send_mail = send_mail

        # Invite result.
        self.email_id = None
        self.url = None
        self.error = None

