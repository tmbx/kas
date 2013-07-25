# from system
import logging, time

# from pylons
from pylons import request, url, session as web_session, tmpl_context as c
from pylons.controllers.util import abort, redirect
from routes import url_for
from kascfg.lib.base import BaseController, render
from kascfg.lib.config import load_master_config, save_master_config

# from kpython package
from kbase import PropStore
from kodict import odict
import kfilter

# from kweb package
from kweb_forms import *
import kweb_lib
from kweb_lib import ErrorMsg

# from kas-python
import ksamba

# local imports
import kascfg.lib.strings_kascfg as strings_kascfg
from cfgcommon import cfg_common, get_var, GT

# from teambox-console-setup
#import kplatsetup

# Setup cfg_common.
cfg_common.strings[None] = strings_kascfg.strings
cfg_common.request = request

# Dynamic ID for scripts so we can force a reload when wanted. This should
# be incremented at every release at least.
dyn_ress_id = 3

# Log object.
log = logging.getLogger(__name__)

# Validation classes.
class ValidationSambaUNCInvalidException(kfilter.ValidationException):
    pass

class ValidationOrgKeyMustBeNumericException(kfilter.ValidationException):
    pass

class ValidationNoSuchOrgKeyException(kfilter.ValidationException):
    pass

class ValidationOrgKeyAlreadyExistsException(kfilter.ValidationException):
    pass

# Constants
NO_FILL = (1<<0)
HIDE_FORM = (1<<1)

# Abstract config controller.
class AbstractKasConfigController(BaseController):
    
    # List of actions that require authentication.
    require_auth = ['show']

    # Configuration template.
    config_template = '/config/show.mako'

    # Map used to convert validation exceptions to strings showable to the end-user.
    validation_exceptions_messages = {
        "ValidationNoneValueException" : GT("validations.errors.none_value", "base"),
        "ValidationTypeException" : GT("validations.errors.type_error", "base"),
        "ValidationStringTooLongException" : GT("validations.errors.string_too_long", "base"),
        "ValidationStringTooShortException" : GT("validations.errors.string_too_short", "base"),
        "ValidationIntTooHighException" : GT("validations.errors.int_too_high", "base"),
        "ValidationIntTooLowException" : GT("validations.errors.int_too_low", "base"),
        "ValidationInvalidChoiceException" : GT("validations.errors.invalid_choice", "base"),
        "ValidationNotPositiveNumberException" : GT("validations.errors.not_positive_number"),
        "ValidationOrgKeyMustBeNumericException" : GT("validations.error.org_key_must_be_numeric"),
        "ValidationNoSuchOrgKeyException" : GT("validations.error.no_such_org_key"),
        "ValidationOrgKeyAlreadyExistsException" : GT("validations.error.org_key_already_exists"),
        "ValidationVerificationFieldException" : GT("validations.errors.verification_failed", "base")
    }

    # Populate 'forms' with the forms and template stores used by the
    # configuration page.
    def get_config_page_forms(self, forms, requested_form):
        pass

    # Forms processing informations.
    # Keys: form identifier
    # Values: ('update_config_method', flags, show_error_in_form)
    process_form_dict = { }

    # Process the configuration page posted form, if any.
    def process_config_page_posted_form(self, forms, form_name):
       
        # Return if system is in production mode.
        if c.mc.production_mode:
            return
 
        # Check if something was posted.
        if get_var('action') != "update": 
            # Nothing posted.
            return
        
        # Get custom form if needed.
        show_errors_in_form = self.process_form_dict[form_name][2]
        
        # Validate the posted form, if required.
        fill_form = None
        if not (self.process_form_dict[form_name][1] & NO_FILL):
            
            # No such form, bail out.
            if not forms.forms.has_key(form_name): return
            
            # Fill the form with the posted variables.
            fill_form = forms.forms[form_name]
            fill_form.fill(request.params)
            
            # Convert exceptions (if any) to strings using the map.
            fill_form.localize_validation_exceptions(self.validation_exceptions_messages)
            
            # The form is not valid, bail out.
            if not fill_form.valid(): return
        
        try:
            # Dispatch form update.
            method = getattr(self, self.process_form_dict[form_name][0])
            method(fill_form)
            
            # Update the configuration.
            save_master_config(c.mc)

            # Re-load services status after the change.
            tmp_mc = load_master_config()
            for name, service in c.services.items():
                service.update_from_conf(tmp_mc)

            if fill_form:
                fill_form.confirmations = ['Changes have been saved.']
            
            # Hide form if flag set.
            if self.process_form_dict[form_name][1] & HIDE_FORM and fill_form:
                fill_form.show = 0
            
        except ErrorMsg, e:
            msg = str(e)
            
            if show_errors_in_form:
                s = c.template_store[show_errors_in_form]
                s.error = msg

            elif fill_form:
                fill_form.notices += [msg]

    ##### ACTIONS #####

    # Main handler.
    def show(self):

        ## Check that server is in maintenance mode.
        #if c.mc.production_mode:
        #    # Redirect to the status page.
        #    return redirect_to(url_for('status'))

        # Push variables to template.
        c.GT = GT
        c.dyn_ress_id = dyn_ress_id
        c.template_store = {}
 
        # Determine which form has been requested or posted, if any.
        requested_form = get_var('form') or ""
        
        # Setup the configuration page forms.
        forms = Forms()
        self.get_config_page_forms(forms, requested_form)
        
        # Process the configuration page posted form, if any.
        self.process_config_page_posted_form(forms, requested_form)
       
        # Render the result.
        return render(self.config_template)

