import logging

from pylons import request, url, config, response, session, tmpl_context as c
from pylons.controllers.util import abort, redirect
from routes import url_for
from kascfg.lib.base import BaseController, render, ui_flash_error
from kascfg.model.freemium import User
from sqlalchemy import asc, desc
import xmlrpclib
import math

log = logging.getLogger(__name__)

class UserManagementController(BaseController):
    require_auth = ['show', 'apply', 'add']

    def show(self):
        if not (c.services['freemium'].configured and c.services['freemium'].enabled):
            # Disallow access.
            return redirect(url_for('status'))

        # Return a rendered template
        page = 0
        limit = 25
        email_criteria = ''
        license_criteria = ''

        order_by = 'email'
        order_dir = 'asc'
        
        if 'order_by' in request.params:
            order_by = request.params['order_by']
        
        if 'order_dir' in request.params:
            order_dir = request.params['order_dir']

        if order_by == 'license':
            order_clause = User.license
        elif order_by == 'email':
            order_clause = User.email
        elif order_by == 'created':
            order_clause = User.created_on

        if order_dir == 'asc':
            order_clause = asc(order_clause)
        else:
            order_clause = desc(order_clause)
            
            
        if 'page' in request.params:
            try:
                tmp_page = int(request.params['page'])
                if tmp_page > 0:
                    page = tmp_page - 1
            except:
                pass

        if 'limit' in request.params:
            try:
                tmp_limit = int(request.params['limit'])
                if tmp_limit > 0:
                    limit = tmp_limit
            except:
                pass
        
        if 'email_criteria' in request.params and request.params['email_criteria'].strip():
            email_criteria = request.params['email_criteria'].strip()

        if 'license_criteria' in request.params:
            license_criteria = request.params['license_criteria']
        
        offset = page * limit
        query = User.query
        
        if email_criteria:
            query = query.filter(User.email.like('%' + email_criteria + '%'))
        
        if (not license_criteria) or license_criteria == 'all':
            pass
        elif license_criteria == 'except_none':
            query = query.filter(User.license !='none')
        else:
            query = query.filter(User.license == license_criteria)
       
        query = query.filter(User.org_id == self.getFreemiumOrgId())
                
        c.users = query.order_by(order_clause).limit(limit).offset(offset)
        c.users_count = query.count()
        c.page_count = int(math.ceil(c.users_count/float(limit)))
        c.select_license_criteria = {'all':'', 'except_none':'', 'none':'', 'confirm':'', 'gold':'', 'freemium':'', 'bronze':'', 'silver':''}
        c.license_criteria = license_criteria
        c.select_license_criteria[license_criteria] = 'selected'
        c.email_criteria = email_criteria
        c.page = page + 1
        c.order_by = order_by
        c.order_dir = order_dir
           
        c.limit = limit

        return render('/user_management/show.mako')
 
    def add(self):
        params = request.params
        if (params['email'] and params['license'] and params['password']):
            self.rpc_server, self.rpc_sid, self.str_org_id = self.connectToRPC()
            self.add_new_user(params['email'], params['password'], params['license'])
        else:
            ui_flash_error(message="Email address and password fields cannot be blank.")
        
        redirect(url('manage_users', limit = params['limit']))
        pass
 
    def apply(self):
        user_ids = []
        apply_action = ''
        params = request.params
       
        for param in params:
            if param.startswith("user_record_"):
                user_ids.append(int(params[param]))

        apply_action = params['apply_action']
        license_action = params['license_action']
        valid = (len(user_ids) and (license_action == 'gold' or license_action =='none' or license_action =='freemium' or license_action =='bronze' or license_action =='silver'))
        valid = valid or (len(user_ids) and apply_action == 'synchronize')
        valid = valid or (len(user_ids) and ((apply_action == 'set_note' and params['note']) or (apply_action == 'set_password' and params['password'])))
        
        if valid:
            # connect to kps xml-rpc operator
            self.rpc_server, self.rpc_sid, self.str_org_id = self.connectToRPC()
            
            if apply_action == 'set_password':
                self.update_users_password(user_ids, params['password'])
            elif apply_action == 'set_note':
                self.update_users_note(user_ids, params['note'])
            elif apply_action == 'synchronize':
                self.sync_users(user_ids)
            else:
                self.update_users_license(user_ids, license_action)
        
        
        redirect(url('manage_users', page=params['page'], limit=params['limit'], email_criteria=params['email_criteria'], license_criteria=params['license_criteria'], order_by=params['order_by'], order_dir=params['order_dir']))
        pass
        
    # private methods
    def update_users_license(self, user_ids, license):
        for id in user_ids:
            user = User.get_by(id = id)
            if user:
                self.rpc_server.set_freemium_user(self.rpc_sid, self.str_org_id, user.email, user.pwd, license, '', '', False)

    def update_users_password(self, user_ids, pwd):
        for id in user_ids:
            user = User.get_by(id = id)
            if user:
                self.rpc_server.set_freemium_user(self.rpc_sid, self.str_org_id, user.email, pwd, user.license, '', user.note, True)
    
    def update_users_note(self, user_ids, note):
        for id in user_ids:
            user = User.get_by(id = id)
            if user:
                self.rpc_server.set_freemium_user(self.rpc_sid, self.str_org_id, user.email, user.pwd, user.license, '', note, True)

    
    def sync_users(self, user_ids):
        for id in user_ids:
            user = User.get_by(id = id)
            if user:
                self.rpc_server.set_freemium_user(self.rpc_sid, self.str_org_id, user.email, user.pwd, user.license, '', '', False)

    
    def add_new_user(self, email, pwd, license):
        self.rpc_server.set_freemium_user(self.rpc_sid, self.str_org_id, email, pwd, license, '', '', False)
        
        
    ######################
    #  Helper Functions  #
    ######################
    def getFreemiumOrgId(self):
        if 'freemium_org_id' not in config:
            rpc_server, rpc_sid, str_org_id = self.connectToRPC()
            config['freemium_org_id'] = str_org_id
            log.info('getting freemium org_id from kps')
            
        return config['freemium_org_id']
    

    def connectToRPC(self):
        rpc_server = xmlrpclib.ServerProxy(config['rpc_url'])
        rpc_sid = rpc_server.session_login(config['rpc_login'], config['rpc_password'])
        str_org_id = rpc_server.get_security_context_org_id(rpc_sid)
        return rpc_server, rpc_sid, str_org_id
