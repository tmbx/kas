import kanp
from kwmo.lib.kwmo_kcd_client import KcdClient
from pylons import config 

from kwmo.lib.config import get_cached_kcd_external_conf_object
from kwmo.model.kcd.kcd_user import KcdUser

#KANP_EMAIL_NOTIF_FLAG = 1
#KANP_EMAIL_SUMMARY_FLAG = 2  
from kflags import Flags

class UserWorkspaceSettings:
    def __init__(self, user_id, workspace_id):
        self._user_id = int(user_id)
        self._workspace_id = int(workspace_id)
        
        #initialize to unloaded
        self._loaded = False
        
        self._notif_policy = 0
        
        self._new_notif_policy = 0
        pass
    
    def save(self):
        
        kc = KcdClient(get_cached_kcd_external_conf_object())

        # TODO: check for status code from Kcd, and handle errors
        kc.save_notification_policy(self._workspace_id, self._user_id, self._new_notif_policy)
        self._notif_policy = self._new_notif_policy
        pass
        
    def load(self):
        if not self._loaded:
            #TODO: bullet-proof code, assert kcd_user and return an error code in case of none
            kcd_user = KcdUser.get_by(user_id = self._user_id, kws_id = self._workspace_id)
            self._notif_policy = kcd_user.notif_policy
            self._loaded = True
            pass
            
    def setNotificationsEnabled(self, value):
        if value:
            self._new_notif_policy = self._new_notif_policy | kanp.KANP_EMAIL_NOTIF_FLAG
        pass

    def setSummaryEnabled(self, value):
        if value:
            self._new_notif_policy = self._new_notif_policy | kanp.KANP_EMAIL_SUMMARY_FLAG
        pass
        
    def getNotificationsEnabled(self):
        self.load()
        return (self._notif_policy &  kanp.KANP_EMAIL_NOTIF_FLAG) == kanp.KANP_EMAIL_NOTIF_FLAG
    
    def getSummaryEnabled(self):
        self.load()
        return (self._notif_policy &  kanp.KANP_EMAIL_SUMMARY_FLAG) == kanp.KANP_EMAIL_SUMMARY_FLAG
    
