# Import abstract config controller.
from kascfg.controllers.abstract_config import *

# from kas-python
import ksamba

import kfilter

# Log object.
log = logging.getLogger(__name__)

class ValidationSmbUNCException(kfilter.ValidationException):
    pass

# Config controller for MAS.
class MasConfigController(AbstractKasConfigController):
    
    # List of actions that require authentication.
    require_auth = ['show']

    # Configuration template.
    config_template = '/config/mas_show.mako'

    # Map used to convert validation exceptions to strings showable to the end-user.
    validation_exceptions_messages = dict(AbstractKasConfigController.validation_exceptions_messages)
    validation_exceptions_messages['ValidationOrgKeyMustBeNumericException'] = \
        GT("validations.error.org_key_must_be_numeric")
    validation_exceptions_messages['ValidationNoSuchOrgKeyException'] = \
        GT("validations.error.no_such_org_key")
    validation_exceptions_messages['ValidationOrgKeyAlreadyExistsException'] = \
        GT("validations.error.org_key_already_exists")
    validation_exceptions_messages['ValidationOrgKeyAlreadyExistsException'] = \
        GT("validations.error.org_key_already_exists")
    validation_exceptions_messages['ValidationSmbUNCException'] = \
        "Invalid share UNC."

    # Check if key ID is numeric and convert it.
    def filter_key_id(self, value):
        if not value.isdigit():
            return FilterResult(value=value, validation_exceptions=[ValidationOrgKeyMustBeNumericException()], 
                continue_filtering=False)
        new_value = int(value)
        return FilterResult(value=int(value))

    # Check if samba unc is valid.
    def filter_smb_unc(self, value):
        if value and value != '':
            try:
                ksamba.samba_path_to_components(value)
            except ksamba.SmbParseException, e:
                return FilterResult(value=value, validation_exceptions=[ValidationSmbUNCException()],
                    continue_filtering=False)
        return FilterResult(value=value)

    # Get KFS Directory.
    def get_kfs_dir(self, mc):
        kfs_dir = "/var/teambox/kas/kfs/"

        if mc.kcd_kfs_mode == "samba":
            kfs_dir = mc.kcd_smb_mount_point + mc.kcd_smb_root

        if not kfs_dir.endswith('/'): kfs_dir += '/'

        return kfs_dir

    # Get smb UNC.
    def get_smb_unc(self, mc):
        unc = ''
        if mc.kcd_smb_mount_unc:
            server, share, dirs = ksamba.samba_path_to_components(mc.kcd_smb_mount_unc)
            unc = '//%s/%s' % ( server, share )
            unc += mc.kcd_smb_root
        unc = unc.replace('/', '\\')
        return unc

    # Check if key ID already exists in configuration.
    def org_key_id_exists(self, key_id):
        return c.mc.kcd_organizations.has_key(key_id)

    # Check if key ID already exist (filter).
    def filter_key_id_exists(self, value):
        if not self.org_key_id_exists(value):
            return FilterResult(value=value, validation_exceptions=[ValidationNoSuchOrgKeyException()], 
                continue_filtering=False)
        return FilterResult(value=value)

    # Check that key ID is new (filter).
    def filter_key_id_new(self, value):
        if self.org_key_id_exists(value):
            return FilterResult(value=value, validation_exceptions=[ValidationOrgKeyAlreadyExistsException()], 
                continue_filtering=False)
        return FilterResult(value=value)
 
    # Populate 'forms' with the forms and template stores used by the
    # configuration page.
    def get_config_page_forms(self, forms, requested_form):

        # Create a template store for the specified form.
        def create_form_store(name):
            c.template_store[name] = odict()
            s = c.template_store[name]
            s.action_url = url_for('mas_config')
            s.forms = forms
            s.form_name = name
            return s
      
        # Determine whether the edit or add organization forms have been
        # requested and whether we are configuring for samba.
        kcd_edit_org_flag = requested_form == "kcd_edit_org"
        kcd_add_org_flag = requested_form == "kcd_add_org"
        if requested_form == "kcd_basic_options":
            samba_flag = get_var("kcd_kfs_mode") == "samba"
        else:
            samba_flag = c.mc.kcd_kfs_mode == "samba"
        
        # Create basic MAS options form.
        create_form_store("kcd_basic_options")
        form = forms.append_form(Form("kcd_basic_options"))
        if c.mc.production_mode: form.read_only = True
        form.append_field(CheckBoxField("mas_service", reference="mas_service"))
        form.append_field(CheckBoxField("kcd_enforce_restriction", reference="kcd_enforce_restriction"))
        form.append_field(TextField("kcd_host", reference="kcd_host", required=True))
        form.append_field(TextField("kwmo_host", reference="kwmo_host", required=True))
        form.append_field(TextField("kcd_mail_sender", reference="kcd_mail_sender", required=True))
        form.append_field(TextField("kcd_mail_host", reference="kcd_mail_host", required=True))
        form.append_field(TextField("kcd_mail_auth_user", reference="kcd_mail_auth_user", required=False))
        form.append_field(PasswordTextField("kcd_mail_auth_pwd", reference="kcd_mail_auth_pwd", required=False))
        form.append_field(CheckBoxField("kcd_mail_auth_ssl", reference="kcd_mail_auth_ssl", required=False))
        kcd_kfs_modes = odict()
        kcd_kfs_modes["local"] = GT("forms.kcd_basic_options.kcd_kfs_mode.choices.local")
        kcd_kfs_modes["samba"] = GT("forms.kcd_basic_options.kcd_kfs_mode.choices.samba")
        form.append_field(SelectField("kcd_kfs_mode", reference="kcd_kfs_mode", required=True, choices=kcd_kfs_modes))

        # Fill basic MAS options with current configuration.
        form.fields["mas_service"].fill(c.mc.mas_service)
        form.fields["kcd_enforce_restriction"].fill(c.mc.kcd_enforce_restriction)
        form.fields["kcd_host"].fill(c.mc.kcd_host)
        form.fields["kwmo_host"].fill(c.mc.kwmo_host)
        form.fields["kcd_mail_host"].fill(c.mc.kcd_mail_host)
        form.fields["kcd_mail_sender"].fill(c.mc.kcd_mail_sender)
        form.fields["kcd_mail_auth_user"].fill(c.mc.kcd_mail_auth_user)
        form.fields["kcd_mail_auth_pwd"].fill(c.mc.kcd_mail_auth_pwd)
        form.fields["kcd_mail_auth_ssl"].fill(c.mc.kcd_mail_auth_ssl)
        form.fields["kcd_kfs_mode"].fill(c.mc.kcd_kfs_mode)

        # Create KFS options form.
        create_form_store("kcd_kfs_options")
        form = forms.append_form(Form("kcd_kfs_options", show_required_notice=False))
        if c.mc.production_mode: form.read_only = True
        if samba_flag:
            form.append_field(TextField("kcd_smb_unc", reference="kcd_smb_unc", required=True,
                                        post_filter_callables=[self.filter_smb_unc]))
            form.append_field(TextField("kcd_smb_mount_user", reference="kcd_smb_mount_user", required=True))
            form.append_field(PasswordTextField("kcd_smb_mount_pwd", reference="kcd_smb_mount_pwd"))
            form.append_field(PasswordTextField("kcd_smb_mount_pwd_verif", reference="kcd_smb_mount_pwd_verif", 
                              verification_field_id='kcd_smb_mount_pwd'))

        # Fill KFS options with current configuration.
        if samba_flag:
            form.fields["kcd_smb_unc"].fill(self.get_smb_unc(c.mc))
            form.fields["kcd_smb_mount_user"].fill(c.mc.kcd_smb_mount_user)
            form.fields["kcd_smb_mount_pwd"].fill(c.mc.kcd_smb_mount_pwd)
            form.fields["kcd_smb_mount_pwd_verif"].fill(c.mc.kcd_smb_mount_pwd)

        # Create organizations list form.
        # Note: it's not really a form, but it is used for convenience.
        col_store = create_form_store("kcd_org_list")
        col_store.kcd_edit_org_flag = kcd_edit_org_flag
        col_store.kcd_add_org_flag = kcd_add_org_flag
        col_store.organizations = c.mc.kcd_organizations
        col_store.error = None
        form = Form("kcd_org_list")
        if c.mc.production_mode: form.read_only = True
        forms.append_form(form)
        
        # Create organization edition form, if needed.
        create_form_store("kcd_edit_org")
        if kcd_edit_org_flag:
            form = forms.append_form(Form("kcd_edit_org", show_required_notice=False, anchor="foid_kcd_org_list_start", 
                                          show_legend=False, show_title=False))
            if c.mc.production_mode: form.read_only = True
            org_key_id = get_var("org_key_id") or ""
            form.append_field(TextField("org_key_id", reference="org_key_id", required=True, 
                                        post_filter_callables=[self.filter_key_id, self.filter_key_id_exists], 
                                        force_value=True, value=org_key_id))
            form.append_field(TextField("org_name", reference="org_name", required=True))

            # Fill fields with current values
            form.fields["org_name"].fill(c.mc.kcd_organizations[int(org_key_id)])
            
        # Create organization adding form, if needed.
        create_form_store("kcd_add_org")
        if kcd_add_org_flag:
            form = forms.append_form(Form("kcd_add_org", show_required_notice=False, anchor="foid_kcd_org_list_start", 
                                          show_legend=False, show_title=False))
            if c.mc.production_mode: form.read_only = True
            form.append_field(TextField("org_key_id", reference="org_key_id", required=True, 
                                        post_filter_callables=[self.filter_key_id, self.filter_key_id_new]))
            form.append_field(TextField("org_name", reference="org_name", required=True))

    # Update basic KCD options.
    def config_kcd_basic_options_update(self, form):
        
        # Update current config.
        c.mc.mas_service = int(form.fields["mas_service"].value)
        c.mc.kcd_enforce_restriction = int(form.fields["kcd_enforce_restriction"].value)
        c.mc.kcd_host = form.fields["kcd_host"].value
        c.mc.kwmo_host = form.fields["kwmo_host"].value
        c.mc.kcd_mail_host = form.fields["kcd_mail_host"].value
        c.mc.kcd_mail_sender = form.fields["kcd_mail_sender"].value
        c.mc.kcd_mail_auth_user = form.fields["kcd_mail_auth_user"].value
        c.mc.kcd_mail_auth_pwd = form.fields["kcd_mail_auth_pwd"].value
        c.mc.kcd_mail_auth_ssl = int((form.fields["kcd_mail_auth_ssl"].value == "on"))
        c.mc.kcd_kfs_mode = form.fields["kcd_kfs_mode"].value
        c.mc.kcd_kfs_dir = self.get_kfs_dir(c.mc)

    # Update KCD KFS options.
    def config_kcd_kfs_options_update(self, form):
        if c.mc.kcd_kfs_mode == "samba":
            smb_unc = form.fields["kcd_smb_unc"].value
            if smb_unc != '':
                smb_unc = smb_unc.replace('\\', '/')
                server, mount, dirs = ksamba.samba_path_to_components(smb_unc)
                c.mc.kcd_smb_mount_unc = '//%s/%s' % ( server, mount )
                c.mc.kcd_smb_root = '/' + '/'.join(dirs)
            else:
                c.mc.kcd_smb_mount_unc = ''
                c.mc.kcd_smb_root = ''

        c.mc.kcd_kfs_dir = self.get_kfs_dir(c.mc)
        c.mc.kcd_smb_mount_user = form.fields["kcd_smb_mount_user"].value
        c.mc.kcd_smb_mount_pwd = form.fields["kcd_smb_mount_pwd"].value
            
    # Add or edit a KCD organization.
    def config_kcd_edit_or_add_org_update(self, form):
        
        # Update current config.
        org_key_id = int(form.fields["org_key_id"].value)
        org_name = form.fields["org_name"].value
        c.mc.kcd_organizations[org_key_id] = org_name

    # Remove a KCD organization.
    def config_kcd_remove_org_update(self, form):

        # Update current config.
        org_key_id = int(get_var("org_key_id"))
        if c.mc.kcd_organizations.has_key(org_key_id):
            del c.mc.kcd_organizations[org_key_id]

    # Forms processing informations.
    # Keys: form identifier
    # Values: ('update_config_method', flags, show_error_in_form)
    process_form_dict = {
        'kcd_basic_options' : ('config_kcd_basic_options_update', 0, None),
        'kcd_kfs_options' : ('config_kcd_kfs_options_update', 0, None),
        'kcd_edit_org' : ('config_kcd_edit_or_add_org_update', HIDE_FORM, 'kcd_org_list'),
        'kcd_add_org' : ('config_kcd_edit_or_add_org_update', HIDE_FORM, 'kcd_org_list'),
        'kcd_remove_org' : ('config_kcd_remove_org_update', NO_FILL, 'kcd_org_list')
        }

