# Import abstract config controller.
from kascfg.controllers.abstract_config import *

# from kpython
from kfilter import filter_positive_number

# Log object.
log = logging.getLogger(__name__)

# Configuration controller for Freemium.
class FreemiumConfigController(AbstractKasConfigController):
    
    # List of actions that require authentication.
    require_auth = ['show']

    # Configuration template.
    config_template = '/config/freemium_show.mako'

    # Populate 'forms' with the forms and template stores used by the
    # configuration page.
    def get_config_page_forms(self, forms, requested_form):

        # Create a template store for the specified form.
        def create_form_store(name):
            c.template_store[name] = odict()
            s = c.template_store[name]
            s.action_url = url_for('freemium_config')
            s.forms = forms
            s.form_name = name
            return s

        # Create basic WPS options form.
        create_form_store("freemium_basic_options")
        form = forms.append_form(Form("freemium_basic_options"))
        if c.mc.production_mode: form.read_only = True
        form.append_field(CheckBoxField("freemium_service", reference="freemium_service"))
        form.append_field(CheckBoxField("freemium_autoregister", reference="freemium_autoregister"))
        # Delay until it is configurable in KPS web interface too.
        #form.append_field(TextField("freemium_org_id", reference="freemium_org_id", required=False,
        #                  post_filter_callables=[filter_positive_number]))
        form.append_field(TextField("kcd_host", reference="kcd_host", required=True))
        form.append_field(PasswordTextField("kcd_pwd", reference="kcd_pwd"))
        form.append_field(PasswordTextField("kcd_pwd_verif", reference="kcd_pwd_verif",
                                            verification_field_id='kcd_pwd'))

        # Fill basic WPS options with current configuration.
        form.fields["freemium_service"].fill(c.mc.freemium_service)
        form.fields["freemium_autoregister"].fill(c.mc.freemium_autoregister)
        # Delay until it is configurable in KPS web interface too.
        #form.fields["freemium_org_id"].fill(str(c.mc.freemium_org_id))
        form.fields["kcd_host"].fill(c.mc.kcd_host)
        form.fields["kcd_pwd"].fill(c.mc.kcd_pwd)
        form.fields["kcd_pwd_verif"].fill(c.mc.kcd_pwd)

    # Update basic WPS options with form values.
    def config_freemium_basic_options_update(self, form):

        # Special validation.
        pwd = form.fields["kcd_pwd"].value
        if pwd.find('"') != -1 or pwd.find("'") != -1: raise ErrorMsg("the password may not contain quote characters")

        # Update configuration.
        c.mc.freemium_service = int(form.fields["freemium_service"].value)
        c.mc.freemium_autoregister = int(form.fields["freemium_autoregister"].value)
        # Delay until it is configurable in KPS web interface too.
        #c.mc.freemium_org_id = long(form.fields["freemium_org_id"].value)
        c.mc.kcd_host = form.fields["kcd_host"].value
        c.mc.kcd_pwd = pwd

    # Forms processing informations.
    # Keys: form identifier
    # Values: ('update_config_method', flags, show_error_in_form)
    process_form_dict = {
        'freemium_basic_options' : ('config_freemium_basic_options_update', 0, None),
        }

