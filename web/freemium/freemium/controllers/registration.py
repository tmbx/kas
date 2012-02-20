import logging
import xmlrpclib

from pylons import config, request, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to, url_for

from freemium.lib.base import BaseController, render
from freemium.model import User

import kbase
from  freemium.lib.freemium_kcd_client import FreemiumKcdClient

from kcd_lib import get_kcd_external_conf_object

log = logging.getLogger(__name__)
import time
from kbase import gen_random

class RegistrationController(BaseController):

    def verify(self, email):
        tmp_usr = User.get_by(email = email)
        if tmp_usr:
            if (tmp_usr.license == 'confirm'):
                # license type is still 'confirm', means that the user has not clicked the confirmation link yet.
                return 'confirm'
            elif (tmp_usr.license == 'freemium' or tmp_usr.license == 'gold' or tmp_usr.license == 'bronze' or tmp_usr.license == 'silver'):
                return 'ok'
            else:
                response.status = 405
                return 'Unable to register this user.'
        else:
            response.status = 404
            return 'Error: Invalid user name'
        
    def confirm(self, email, nonce):
        enable_register = c.mc.freemium_autoregister
        c.page_title = "Email confirmation"
        if not enable_register:
            response.status = 403
            c.message = 'The free registration is currently not available. Please try again later.'
        else:            
            tmp_usr = User.get_by(email = email)
            if tmp_usr and tmp_usr.license=='confirm' and tmp_usr.nonce == nonce:
                # Update user in freemium and kps DB
                rpc_server, rpc_sid, str_org_id = self.connectToRPC()
                rpc_server.set_freemium_user(rpc_sid, str_org_id, tmp_usr.email, tmp_usr.pwd, 'freemium' , nonce, '', False)
                c.success = True
            else:
                response.status = 403
                c.success = False
        return render('/registration/confirm.mako')
        
    def create(self):
        email = ''
        pwd = ''
        
        if ( ('email' in request.params) and ('pwd' in request.params)):
            email = request.params['email']
            pwd = request.params['pwd']
        
        if email and pwd:

            user_email = email
            user_pwd = pwd
            user_license =''
            user_nonce = ''

            tmp_usr = User.get_by(email = user_email)

            if (tmp_usr):
                user_license = tmp_usr.license
                user_nonce = tmp_usr.nonce
                user_pwd = tmp_usr.pwd

            if (user_license == 'none'):
                response.status = 403
                return 'user_registration_locked'

            elif ((user_license == '') or (user_license == 'confirm')):
                
                enable_register = c.mc.freemium_autoregister
                if not enable_register:
                    response.status = 403
                    return 'registration_disabled'
               
                if (user_nonce == '' or (pwd != user_pwd)):
                    user_nonce = self.generateNonce()
                    user_pwd = pwd
                
                user_license = 'confirm'

                # Send confirmation email
                confirm_link = url_for('confirm', email= user_email , nonce = user_nonce, qualified=True)
                log.debug("confirm link:  " + confirm_link)
                
                conf = get_kcd_external_conf_object(master_config=c.mc)

                if not conf:
                    response.status = 405
                    return 'The free registration is not available. Please conact your system adminstrator.'

                try:
                    kc = FreemiumKcdClient(conf)
                    kc.send_freemium_confirmation_email(conf.kcd_passwd, user_email, confirm_link)
                except Exception, err:
                    log.error(str(err))
                    response.status = 500
                    return 'Teambox Main Application Server error'

                # Update user in freemium and kps DB
                try:
                    rpc_server, rpc_sid, str_org_id = self.connectToRPC()
                    rpc_server.set_freemium_user(rpc_sid, str_org_id, user_email, user_pwd, user_license , user_nonce, '', False)
                except Exception, err:
                    log.error(str(err))
                    response.status = 500
                    return 'Teambox Sign-on Server error'
                    
                
                return 'confirm'
                
            elif ((user_license == 'freemium') or (user_license == 'gold') or (user_license == 'bronze') or (user_license == 'silver')):
                if (pwd == user_pwd):
                    return 'ok'
                else:
                    response.status = 403
                    return 'user_login_taken'
            else:
                response.status = 405
                return 'Invalid license type'
    
        else:
            response.status = 405
            return 'Missing params: email and/or password.'

    ######################
    #  Helper Functions  #
    ######################

    def connectToRPC(self):
        rpc_server = xmlrpclib.ServerProxy(config['rpc_url'])
        rpc_sid = rpc_server.session_login(config['rpc_login'], config['rpc_password'])
        str_org_id = rpc_server.get_security_context_org_id(rpc_sid)
        return rpc_server, rpc_sid, str_org_id
    
    def generateNonce(self):
        NONCE_LENGTH = 25
        return gen_random(NONCE_LENGTH)
        
        
