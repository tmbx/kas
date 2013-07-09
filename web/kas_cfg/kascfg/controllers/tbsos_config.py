# Import abstract config controller.
from kascfg.controllers.abstract_config import *

# Log object.
log = logging.getLogger(__name__)

# Configuration controller for WPS.
class TbsosConfigController(AbstractKasConfigController):
    
    # List of actions that require authentication.
    require_auth = ['show']

    # Configuration template.
    config_template = '/config/tbsos_show.mako'

    # Populate 'forms' with the forms and template stores used by the
    # configuration page.
    def get_config_page_forms(self, forms, requested_form):

        # Create a template store for the specified form.
        def create_form_store(name):
            c.template_store[name] = odict()
            s = c.template_store[name]
            s.action_url = url_for('tbsos_config')
            s.forms = forms
            s.form_name = name
            return s

        # Create basic WPS options form.
        create_form_store("tbsos_basic_options")
        form = forms.append_form(Form("tbsos_basic_options"))
        if c.mc.production_mode: form.read_only = True
        form.append_field(CheckBoxField("tbsos_service", reference="tbsos_service"))
        form.append_field(TextField("kcd_host", reference="kcd_host", required=True))
        #form.append_field(PasswordTextField("tbsos_pwd", reference="tbsos_pwd"))
        #form.append_field(PasswordTextField("tbsos_pwd_verif", reference="tbsos_pwd_verif",
        #                                    verification_field_id='tbsos_pwd'))

        # Fill basic WPS options with current configuration.
        form.fields["tbsos_service"].fill(c.mc.tbsos_service)
        form.fields["kcd_host"].fill(c.mc.kcd_host)
        #form.fields["tbsos_pwd"].fill(c.mc.tbsos_pwd)
        #form.fields["tbsos_pwd_verif"].fill(c.mc.tbsos_pwd)

    # Update basic WPS options with form values.
    def config_tbsos_basic_options_update(self, form):

        # Special validation.
        #pwd = form.fields["tbsos_pwd"].value
        #if pwd.find('"') != -1 or pwd.find("'") != -1: raise ErrorMsg("the password may not contain quote characters")

        # Update configuration.
        c.mc.tbsos_service = int(form.fields["tbsos_service"].value)
        c.mc.kcd_host = form.fields["kcd_host"].value
        #c.mc.tbsos_pwd = pwd

    # Forms processing informations.
    # Keys: form identifier
    # Values: ('update_config_method', flags, show_error_in_form)
    process_form_dict = {
        'tbsos_basic_options' : ('config_tbsos_basic_options_update', 0, None),
        }

    def redirect(self):
        # Load config.
        master_conf = load_master_config()

        # Get tbsos host.
        host = master_conf.kps_host
        if not host or host == '':
            host = request.environ.get('HTTP_HOST')
            if not host or host == '':
                host = request.environ.get('SERVER_NAME')
        host = host.split(':')[0]

        # Redirect to that interafce.
        return redirect('https://'+host+':9000')

