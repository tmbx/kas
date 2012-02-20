from kwmo.model.kcd.meta import metadata, Session
__metadata__ = metadata
__session__ = Session
from elixir import *

class KcdPubEmailInfo(Entity):
    # Options
    using_options(shortnames=True, tablename="kcd_kws_pub_email_info", autoload=True)

