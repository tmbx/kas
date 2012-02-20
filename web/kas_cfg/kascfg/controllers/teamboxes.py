# from system
import logging, hashlib, time

# from pylons
from pylons import request, url, session as web_session, tmpl_context as c
from pylons.controllers.util import abort, redirect_to
from routes import url_for
from kascfg.model.kcd import Session as db_session
from kascfg.lib.base import BaseController, render
from sqlalchemy.exceptions import OperationalError

# from kpython package
from kbase import PropStore
from krun import get_cmd_output
from kodict import odict

# from kweb package
import kweb_lib
from kweb_lib import ErrorMsg

# from kas-python
from kanp import KANP_KWS_FLAG_DELETE

# from local
from cfgcommon import cfg_common, get_var, ltu, GT
import kascfg.lib.strings_kascfg as strings_kascfg
from kascfg.lib.uimessage import ui_error

# Setup cfg_common.
cfg_common.strings[None] = strings_kascfg.strings
cfg_common.request = request

# Dynamic ID for scripts so we can force a reload when wanted. This should
# be incremented at every release at least.
dyn_ress_id = 3

# Log object.
log = logging.getLogger(__name__)

# Controller for the KCD configuration interface.
class TeamboxesController(BaseController):
    
    # List of actions that require authentication.
    require_auth = ['show']

    # This method resets the Teambox management query parameters.
    def reset_kws_mgt_query(self):
        web_session["kws_mgt_query_offset"] = 0;
        web_session["kws_mgt_query_limit"] = 30;
        web_session.save()

    # This method retrieves the values of the Teambox range to display from the
    # posted variables and set them in the session.
    def get_kws_mgt_kws_query_range(self):
        offset = get_var("kws_start")
        limit = get_var("kws_per_page")
        if not offset or not offset.isdigit() or int(offset) < 1: offset = 1
        if not limit or not limit.isdigit() or int(limit) <= 0: limit = 30
        web_session["kws_mgt_query_offset"] = int(offset) - 1;
        web_session["kws_mgt_query_limit"] = int(limit);
        web_session.save()

    # This method returns the list of Teamboxes that have been selected in the
    # posted query.
    def get_kws_mgt_query_sel_kws_list(self):
        l = []
        base_name = "kws_mgt_kws_cb_"
        for name in request.params.keys():
            if name.startswith(base_name):
                kws_id = name[len(base_name):]
                if kws_id.isdigit():
                    l.append(int(kws_id))
        return l

    # This method returns a list containing the ID of the managed Teamboxes in
    # the range specified.
    def get_kws_mgt_kws_list(self, offset, limit):
        l = []
        cur = db_session.execute(("SELECT kws_id FROM kcd_kws_list WHERE (flags & %d = 0) " +\
                                  "ORDER BY kws_id OFFSET %i LIMIT %i") % \
                                 (KANP_KWS_FLAG_DELETE, offset, limit))
        for row in cur.fetchall(): l.append(row[0])
        db_session.rollback()
        return l

    # This method returns the number of managed Teamboxes.
    def get_kws_mgt_nb_kws(self):
        cur = db_session.execute("SELECT count(kws_id) FROM kcd_kws_list WHERE (flags & %d = 0)" %\
                                 (KANP_KWS_FLAG_DELETE))
        row = cur.fetchone()
        if not row: raise Exception("cannot get number of Teamboxes")
        nb_kws = row[0]
        db_session.rollback()
        return nb_kws

    # This method returns a PropStore object containing information about the
    # Teambox specified.
    def get_kws_mgt_kws_info(self, kws_id):
        ps = PropStore()
        ps.kws_id = kws_id
        ps.org_name = ""
        ps.user_list = []
        
        cur = db_session.execute(("SELECT creation_date, name FROM kcd_kws_list WHERE kws_id = %i") % (kws_id))
        row = cur.fetchone()
        if row == None: raise ErrorMsg("Teambox %i not found" % (kws_id))
        ps.creation_date = row["creation_date"]
        ps.name = ltu(row["name"])
        cur.close()
        
        cur = db_session.execute(("SELECT file_size, file_quota FROM kcd_kws_kfs_limit WHERE kws_id = %i") % (kws_id))
        row = cur.fetchone()
        if row == None: raise ErrorMsg("Teambox %i not found" % (kws_id))
        ps.file_quota = row["file_quota"]
        ps.file_size = row["file_size"]
        cur.close()
        
        cur = db_session.execute(("SELECT user_id, email, name_admin, name_user, org_name FROM " +\
                                  "kcd_kws_users WHERE kws_id = %i ORDER BY user_id") % (kws_id))
        for row in cur.fetchall():
            user = PropStore()
            user.user_id = row["user_id"]
            user.email = ltu(row["email"])
            if row["name_admin"] != "": user.name = ltu(row["name_admin"])
            elif row["name_user"] != "": user.name = ltu(row["name_user"])
            else: user.name = ""
            user.org_name = ltu(row["org_name"])
            ps.user_list.append(user)
        db_session.rollback()
        if len(ps.user_list): ps.org_name = ps.user_list[0].org_name
        return ps

    # This method changes the quota of a Teambox in the database.
    def update_kws_quota(self, kws_id, quota):
        db_session.execute("UPDATE kcd_kws_kfs_limit SET file_quota = %i WHERE kws_id = %i" % (quota, kws_id))
        db_session.commit()

    # This method formats a Teambox date in a user-friendly fashion.
    def format_kws_date(self, date):
        return " ".join(time.asctime(time.localtime(date)).split(" ")[1:])

    # This method expresses a number of bytes in terms of megabytes with the
    # precision specified, as a string.
    def format_as_mb(self, nb, precision):
        return ("%." + str(precision) + "f") % (nb / (1024*1024.0))






















        
    # This method handles the "new Teambox query" management action.
    def kws_mgt_new_query(self):
        self.reset_kws_mgt_query()
        return self.show_kws_mgt_query_page()

    # This method handles the "show query results" management action.
    def kws_mgt_show(self):
        self.get_kws_mgt_kws_query_range()
        return self.show_kws_mgt_query_page()

    # This method handles the "next query results" management action.
    def kws_mgt_next(self):
        self.get_kws_mgt_kws_query_range()
        web_session["kws_mgt_query_offset"] += web_session["kws_mgt_query_limit"]
        web_session.save()
        return self.show_kws_mgt_query_page()

    # This method handles the "last query results" management action.
    def kws_mgt_last(self):
        self.get_kws_mgt_kws_query_range()
        nb_kws = self.get_kws_mgt_nb_kws()
        web_session["kws_mgt_query_offset"] = max(0, nb_kws - web_session["kws_mgt_query_limit"])
        web_session.save()
        return self.show_kws_mgt_query_page()

    # This method handles redirections to the kwmo read-only admin interface.
    # A time-based hash of the database password is used for the authentication.
    def kws_mgt_kwmo_management(self):
        # Get workspace ID.
        kws_id = get_var("kws_id") or abort(404)

        # Get kcd password.
        if c.mc.kcd_pwd and len(c.mc.kcd_pwd):
            passwd = c.mc.kcd_pwd
        else:
            passwd = c.mc.admin_pwd

        # Get current timestamp.
        stamp = int(time.time())

        # Generate a time-based hash of the password.
        md5 = hashlib.md5()
        md5.update(str(stamp))
        md5.update(passwd)
        hex_hash = md5.digest().encode('hex')

        # Generate the url to redirect to.
        url = "https://%s/teambox_admin/login/%s?stamp=%i&hash=%s" % \
            ( c.mc.kwmo_host, str(kws_id), stamp, str(hex_hash) )

        return redirect_to(url)

    # This method is called when a query action has been specified.
    def kws_mgt_query_action(self):
        choice = get_var("kws_mgt_query_action_select")
        if choice == "delete": return self.kws_mgt_delete()
        else: return self.show_kws_mgt_query_page()
        
    # This method handles the "delete Teambox" management action.
    def kws_mgt_delete(self):
        for kws_id in self.get_kws_mgt_query_sel_kws_list():
            get_cmd_output(["sudo", "/usr/bin/kcdhelper", "--delete-kws", str(kws_id)])
        return self.show_kws_mgt_query_page()

    # This method shows the Teambox management query page.
    def show_kws_mgt_query_page(self):
       
        # Define the session variables, if required.
        if not web_session.has_key("kws_mgt_query_offset"):
            self.reset_kws_mgt_query()
        
        # Obtain the Teambox list.
        kws_list = self.get_kws_mgt_kws_list(web_session["kws_mgt_query_offset"], web_session["kws_mgt_query_limit"])
        
        # Obtain the information about the Teamboxes.
        kws_dict = odict()
        for kws_id in kws_list: kws_dict[kws_id] = self.get_kws_mgt_kws_info(kws_id)
        
        # Show the information.
        action_url = url_for('teamboxes')
        
        # Get the Teambox list content.
        s = ""
        for kws_info in kws_dict.values():
            kws_href = action_url + "?kws_mgt_specific_kws=%i" % (kws_info.kws_id)
            s += '    <tr>\n'
            s += '      <td><input type="checkbox" name="kws_mgt_kws_cb_%i"/></td>\n' % (kws_info.kws_id)
            s += '      <td class="kwstableid">%i</td>\n' % (kws_info.kws_id)
            s += '      <td class="kwstablename"><a href="%s">%s</a></td>\n' % \
                 (kws_href, kweb_lib.html_text_escape(kws_info.name))
            s += '      <td class="kwstablestats">%i</td>\n' % (len(kws_info.user_list))
            s += '      <td class="kwstablestats">%s MiB</td>\n' % (self.format_as_mb(kws_info.file_size, 2))
            s += '      <td class="kwstablestats">%s</td>\n' % (self.format_kws_date(kws_info.creation_date))
            s += '      <td class="kwstablestats">%s</td>\n' % (kweb_lib.html_text_escape(kws_info.org_name))
            s += '    </tr>\n'
        kws_table_body = s
       
        # Push variables to template.  
        c.action_url = action_url
        c.kws_table_body = kws_table_body
        c.kws_mgt_query_offset = web_session["kws_mgt_query_offset"] + 1
        c.kws_mgt_query_limit = web_session["kws_mgt_query_limit"]
        
        return render('/teamboxes/query.mako')
        
    # This method handles the Teambox management Teambox-specific actions.
    def kws_mgt_specific(self):
        
        # Get the Teambox ID.
        kws_id = get_var("kws_mgt_specific_kws")
        if not kws_id or not kws_id.isdigit(): raise Exception("no Teambox specified")
        kws_id = int(kws_id)
       
        # Get the Teambox information.
        kws_info = self.get_kws_mgt_kws_info(kws_id)
        
        if get_var('kws_mgt_set_kws_quota'): return self.kws_mgt_set_quota(kws_info)
        else: return self.show_kws_mgt_specific_page(kws_info)

    # This method handles the "set quota" management action.
    def kws_mgt_set_quota(self, kws_info):
        
        # Validate the quota.
        valid_quota = 0
        try:
            current_size = kws_info.file_size
            user_quota = get_var("kws_quota")
            if user_quota == None or not user_quota.isdigit(): raise ValueError
            new_quota = int(user_quota) * 1024*1024
            if new_quota < current_size: raise ValueError
            valid_quota = 1
        except ValueError: pass
            
        # Update the quota.
        if valid_quota:
            self.update_kws_quota(kws_info.kws_id, new_quota)
            kws_info.file_quota = new_quota
        
        # Show the management page.
        return self.show_kws_mgt_specific_page(kws_info)

    # This method shows the Teambox management Teambox-specific page.
    def show_kws_mgt_specific_page(self, kws_info):
        
        action_url = url_for('teamboxes')
        c.kws_id = kws_info.kws_id
        c.action_url = action_url
        c.back_url = action_url + "?kws_mgt_reshow_kws=1"
        c.kws_ro_link = action_url + "?kws_id=%i&kwmo_redir=1" % ( kws_info.kws_id )
        c.kws_name = kws_info.name
        
        c.kws_creator = ""
        if len(kws_info.user_list): c.kws_creator += kws_info.user_list[0].email + " "
        if kws_info.org_name: c.kws_creator += "(" + kws_info.org_name + ")"

        c.kws_date = self.format_kws_date(kws_info.creation_date)
        
        tmp = ""
        for user in kws_info.user_list:
            if user.name: middle = user.name + " (" + user.email + ")"
            else: middle = user.email
            tmp += "    <tr><td>%s</td></tr>\n" % (kweb_lib.html_text_escape(middle))
        c.member_list = tmp
        
        c.kws_file_size = self.format_as_mb(kws_info.file_size, 2)
        c.kws_quota = self.format_as_mb(kws_info.file_quota, 0)
        
        return render('/teamboxes/specific.mako')
        
    
    def show(self):

        if not (c.services['mas'].configured and c.services['mas'].enabled):
            # Disallow access.
            return redirect_to(url_for('status'))

        # Push variables to templates.
        c.GT = GT
        c.dyn_ress_id = dyn_ress_id
 
        try:
            # Make sure KCD database is reachable.
            db_session.execute('SELECT 1')
        except OperationalError:
            ui_error(message="Database connection to MAS could not be established. You might want to check the configuration.")
            return render('/common/message.mako')

        # Dispatch.
        if get_var("kws_mgt_specific_kws"): return self.kws_mgt_specific()
        elif get_var("kws_mgt_query_action"): return self.kws_mgt_query_action()
        elif get_var("kws_mgt_show_kws"): return self.kws_mgt_show()
        elif get_var("kws_mgt_reshow_kws"): return self.show_kws_mgt_query_page()
        elif get_var("kws_mgt_next_kws"): return self.kws_mgt_next()
        elif get_var("kws_mgt_last_kws"): return self.kws_mgt_last()
        elif get_var("kwmo_redir"): return self.kws_mgt_kwmo_management()
        else: return self.kws_mgt_new_query()
        
