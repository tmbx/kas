#import base_Kcd_client
from kcd_client import *

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

class FreemiumKcdClient(BaseKcdClient):
    def send_freemium_confirmation_email(self, root_pwd, email, link):
        # Connect to KCD.
        self.connect()

        # Select workspace role.
        self.select_role(kanp.KANP_KCD_ROLE_WORKSPACE)
        
        # Send the command to KCD.
        m = kanp.ANP_msg()
        m.add_str(root_pwd)
        m.add_str(email)
        m.add_str(link)

        payload = m.get_payload()
        self.send_command_header(kanp.KANP_CMD_MGT_FREEMIUM_CONFIRM, len(payload))
        self.write(payload)
        
        # Get command result.
        h, m = kanp.get_anpt_all(self)

        #assert h.type == kanp.KANP_RES_FAIL
        #raise kanp.KANPFailure(m.get_u32(), m.get_str())
        
        # Close connection
        self.close()
        
        return 0
    

    def set_freemium_user(self, email, license):
        # Connect to database if needed.
        self.db_connect()

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
