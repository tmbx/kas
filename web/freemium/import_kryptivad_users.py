#!/usr/bin/python -u

import kbase
import kanp
import kpg
from kcd_lib import get_kcd_db_conn
from kctllib.kdatabase import *


# TODO import FreemiumKcdClient from the correct place
class KcdDbConnectException(Exception):
    pass
class FreemiumKcdClient:
    # Constructor
    # parameters:
    #  conf: configuration object, or None
    def __init__(self, conf, db_conn=None):
        # Get config object.
        self.conf = conf

        # Get db connection if provited.
        self.db_conn = db_conn

        # Command ID.
        self.command_id = 0

        # Initialize parent class, but do not connect.
        #tcp_client.TcpClient.__init__(self, self.conf.kcd_host, self.conf.kcd_port, use_openssl=True, allow_dh=True)

    # This method connects or reuse a DB connection to KCD.
    def db_connect(self):
        if not self.db_conn: self.db_conn = get_kcd_db_conn(self.conf)

    def set_freemium_user(self, email, license):
        # Connect to database if needed.
        try:
            self.db_connect()
        except Exception, e:
            raise KcdDbConnectException("Couldn't connect to kcd db: " + str(e))

        # Build ANP arguments list.
        m = kanp.ANP_msg()
        m.add_str(email)
        m.add_str(license)

        # Do the query.
        query = "SELECT set_freemium_user(E'%s')" % ( pgdb.escape_bytea(m.get_payload()) )
        cur = kpg.exec_pg_query_rb_on_except(self.db_conn, query)
        row = cur.fetchone()
        self.db_conn.commit()

        return 0


def import_users_to_freemium_db():

    #TODO: Modify kpsapi.ini and read from config file
    f_db_name = "freemium";
    #f_db_username = "freemium_user";
    #f_db_password = "123";
    f_db_host = "/var/run/postgresql";
    f_db_port = "5432";
    f_db_timeout = "5000";

    db_init()
    users = sdb_logins_find()

    # Freemium database changes
    fdb = pgdb.connect(host = f_db_host, database = f_db_name)#,  user = f_db_username, password = f_db_password)
    fcur = fdb.cursor()
    num = 0
    for user in users:
        login_name = user["user_name"]
        password = user["passwd"]
        org_id = user["org_id"]
        #raise "ss"
        try:

            #insert tuple with new params into freemium DB
            query = "insert into freemium_users (org_id, email, pwd, license, nonce) values (%s, '%s', '%s', 'gold', '')" % (org_id, login_name, password)
            fcur.execute(query)
            fdb.commit()
            num = num + 1
        except Exception, e:
            fdb.rollback()
            pass
    fcur.close()
    fdb.close()
    return num

def import_users_to_kcd(kcd_address, kcd_pwd):
    if (not kcd_address) or (not kcd_pwd):
        return 0
    kcd_cfg = kbase.PropStore()

    kcd_cfg.kcd_port = 443
    kcd_cfg.db_port = 5432
    kcd_cfg.db_user = 'kwsfetcher'
    kcd_cfg.db_passwd = kcd_pwd
    kcd_cfg.db_host = kcd_address
    kcd_cfg.kcd_host = ''

    db_init()
    users = sdb_logins_find()

    kc = FreemiumKcdClient(kcd_cfg)
    num = 0
    for user in users:
        login_name = user["user_name"]
        pemail = login_name
        try:
           pemail = sdb_get_prof_pemail(user["prof_id"])
        except Exception, ze:
           pass
        try:
            kc.set_freemium_user(pemail, 'gold')
            num = num + 1
        except KcdDbConnectException, kcde:
            print "Kcd error:" + str(kcde)
            return 0
        except Exception, e:
            #log exception
            pass

    return num
        
def import_users(kcd_address, kcd_pwd):
    print 'Done. %i users imported to freemium db. %i users imported into kcd.' % (import_users_to_freemium_db(), import_users_to_kcd(kcd_address, kcd_pwd))
        
def main():
    kcd_address = ""
    kcd_pwd = ""
    if not (len(sys.argv)==1 or len(sys.argv)==3):
        print "Invalid parameters, exiting"
        return

    if len(sys.argv)==3:
        kcd_address = sys.argv[1]
        kcd_pwd = sys.argv[2]
    import_users(kcd_address, kcd_pwd)

if __name__ == "__main__":
    main()
