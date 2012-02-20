// Delay, before the expiration, for showing the expiration date. 0 means always show.
var pubws_att_expire_warning = 0; // 86400*3;

// Global PubWS modules.
var pubws_chat = null;
var pubws_kfsup = null;
var pubws_kfsdown = null;
var pubws_create = null;

// Global PubWS object.
var pubws = null;

// Global hidden iframe object.
var thf = null;

// This function returns an identity name (if present) or email.
function get_identity_name_or_email(identity, nbsp)
{
    if (nbsp == null) { nbsp = true; }
    if (identity.name && identity.name != '') { return (nbsp ? replace_nbsp(identity.name) : identity.name); }
    else { return identity.email; }
}

// Start KWMO in public Teambox mode.
function kwmo_pubws_init()
{
    DebugConsole.debug(7, null, "kwmo_pubws_init() called.");

    // Hide the no-script comment (for browsers that do not support javascript).
    hide_by_id("noscript");

    // Initialize base kwmo.
    init_kwmo();
    reset_kwmo_state();

    // Instantiate pubws modules. Note: initialization will be done by the PubWS object.
    pubws_chat = new PubWSChat();
    pubws_kfsup = new PubWSKFSUp();
    pubws_kfsdown = new PubWSKFSDown();
    pubws_create = new PubWSCreate();

    // Initiate PubWS.
    pubws = new PubWS();
    pubws.init(kws_id, user_id, new Array(pubws_chat, pubws_kfsup, pubws_kfsdown, pubws_create));

    // Instantiate temp hidden iframe object.
    thf = new TempHiddenIFrame()

    DebugConsole.debug(6, null, "kwmo_pubws_init() finished.");
    //DebugConsole.show();
}

// Callback: got first kwmo update.
function kwmo_first_update()
{
    DebugConsole.debug(9, null, "kwmo_first_update() called.");

    DebugConsole.debug(9, null, "kwmo_first_update() finished.");
}

// Public workspace object
var PubWS = function()
{
    this._workspace_id = null;
    this._modules = null;

    this._email_info = null;
    this._identities = null;
    this._misc_info = null;

    this._perms_updated_once = false;
    this._hide_progress_bar = true;

    // Module callbacks
    this._module_success_callback = null;
    this._module_failure_callback = null;
    this._module_cancel_callback = null;
};

// Log an error using DebugConsole.
PubWS.prototype.error = function(msg) { DebugConsole.error("PubWS", msg); };

// Log debug information using DebugConsole.
PubWS.prototype.debug = function(level, msg) { DebugConsole.debug(level, "PubWS", msg); };

// Initialize object.
PubWS.prototype.init = function(workspace_id, user_id, modules)
{
    this.debug(9, "Initializing...");

    // Status
    this._status = 'idle';
    if (user_id) 
    { 
        this._status = 'ready';

        // Show logout link.
        $('logout_link').show(); 
    }

    // Save arguments.
    this._workspace_id = workspace_id;
    this._modules = modules;

    // Show skurl links.
    $('action_links').show();

    // Read status from html input nodes.
    this._email_info = $('pubws_email_info').value.evalJSON();
    this._identities = $('pubws_identities').value.evalJSON();

    // Update perms.
    state_add_cur_flags(STATE_WANT_PERMS|STATE_WANT_PUBWS_INFO);

    // Init state handlers.
    this.initStateHandlers();

    // Initialize modules.
    for(var i=0; i<this._modules.length; i++) { this._modules[i].init(this); }

    // Get first state from html, and start the update loop.
    state_update_from_html_input();

    this.debug(8, "Initialized.");
};

// Get status.
PubWS.prototype.getStatus = function()
{
    return this._status;
};

// Initialize state handlers.
PubWS.prototype.initStateHandlers = function()
{
    var THIS = this;

    // Add state handlers. Defaults state handlers are set in kwmo.js.
    // Order IS important.
    state_handlers[state_handlers.length] = new Array(STATE_WANT_PUBWS_INFO, function(result) { THIS.updatePubWSInfo(result); });
    state_handlers[state_handlers.length] = new Array(STATE_WANT_PERMS, function(result) { THIS.updatePerms(result); });
    state_handlers[state_handlers.length] = new Array(STATE_WANT_MEMBERS, function(result) { THIS.updateMembers(result); });
};

// Update permissions.
PubWS.prototype.updatePerms = function(res)
{
    try 
    {
        // Update global permissions object.
        update_perms(res);

        // Update permissions in all modules.
        for(var i=0; i<this._modules.length; i++) { this._modules[i].updatePerms(perms); }

        if (!this._perms_updated_once)
        {
            // Show main public workspace links and widgets.
            show_by_id('action_links');
            show_by_id('mail_info');

            this._perms_updated_once = true;
        }
    }
    catch(err) { this.error("updatePerms() error: '" + err + "'."); }
};

// Update members.
PubWS.prototype.updateMembers = function(res)
{
    // Call global update_members.
    try { update_members(res); }
    catch(err) { this.error("updateMembers() error: '" + err + "'."); }
};

// Update PubWS information.
PubWS.prototype.updatePubWSInfo = function(res)
{
    // Update misc pubws info.
    this._misc_info = res['data'];

    // Disable drop-back if attachments have expired.
    pubws_kfsup.reqConditionallyEnable();

    // Refresh kfs view, if ready.
    if (pubws_kfsdown._files && pubws_kfsdown._files['sender']) { pubws_kfsdown.refreshView(); }
};

// Disable module requests.
PubWS.prototype.reqDisable = function(reason)
{
    for(var i=0; i<this._modules.length; i++) { this._modules[i].reqDisable(reason); }
};

// Start identity selection.
PubWS.prototype.reqIdentity = function(module_success_callback, module_failure_callback, module_cancel_callback)
{
    // Save module callbacks.
    this._module_success_callback = module_success_callback;
    this._module_failure_callback = module_failure_callback;
    this._module_cancel_callback = module_cancel_callback;

    if (this._status == 'ready') { this.reqFinishOK(); }

    if (this._status != 'idle')
    {
        // Should not happen, except maybe for quick clicks on the same link, in which case we don't want to do anything anyways.
        this.debug(4, 'request(): status is pending.');
    }

    // Start request.
    this._status = 'pending';

    this.debug(1, "Starting identity selection.");

    if (this._identities.length > 2)
    { 
        // There are more than two identities... let the user choose one. The first identity is the sender, which 
        // is not usable.
        this.showIdentitySelectionBox(); 
    }
    else if (this._identities.length > 1) 
    {
        // There are only two identities, the first being the sender... choose automatically the second one (the 
        // sender cannot login as himself).
        this.selectIdentity(1); 
    }
    else
    {
        // There is only one identity, which means the user sent himself an email, with no other recipient (see 
        // the server side for more info). Choose automatically the first identity (this is a special case 
        // because usually the sender cannot login).
        this.selectIdentity(0);
    }
};

// Show identity selection box.
PubWS.prototype.showIdentitySelectionBox = function()
{
    var THIS = this;

    this.debug(4, "Showing the identity selection widget.");

    // Show a box to the user so he can select his identity (except for identity[0], which is the sender).
    var s = '<div id="identity_selection">\n';
    for (var i=1; i<this._identities.length; i++)
    {
        var identity = this._identities[i];
        s += '<a href="#" onclick="pubws.selectIdentity(' + i + '); return false;">' + identity.email + '</a><br />\n';
    }
    s += '</div>\n';
    Modalbox.show(s, 
        { title:'Please select who you are:', width:500, overlayClose:false, 
          onDialogClosed: function() { THIS._cbIdentitySelectionDialogClosed(); } });
};

// Callback: the identity selection dialog was closed.
PubWS.prototype._cbIdentitySelectionDialogClosed = function()
{
    this.reqCancel();
};

// Select user identity.
PubWS.prototype.selectIdentity = function(i)
{
    var THIS = this;
    this.debug(2, "User has selected an identity.");

    // Re-show status progress bar.
    this.showStatusProgress();

    // Do the request.
    var url = get_url_path('teambox_pubws_set_identity') + '/' + this._workspace_id;
    this.debug(6, "Post identity url: '" + url + "'.");
    new KAjax('set_ident').jsonRequest(url, { 'identity_id' : i }, 
        function(res_dict, transport) { THIS._cbIdentitySelectionSuccess(res_dict, transport); },
        function(transport) { THIS._cbIdentitySelectionFailure(transport); }, 'POST');
};

// Set user ID.
PubWS.prototype.setUserID = function(t_user_id)
{
    // Update global variable user_id (defined in kwmo.js).
    user_id = t_user_id;

    // Update user ID in modules.
    for(var i=0; i<this._modules.length; i++) { this._modules[i].setUserID(this); }

    // Show logout link.
    $('logout_link').show();

    this.debug(1, "setUserID(): user id is " + t_user_id);
};

// Callback: identity was set successfully.
PubWS.prototype._cbIdentitySelectionSuccess = function(res_dict, transport)
{
    var THIS = this;
    var func_name = "_cbIdentitySelectionSuccess";
    try
    {
        this.debug(9, func_name + "(): res_dict: '" + dump(res_dict) + "'.");

        // Set user ID.
        this.setUserID(res_dict["user"]["id"]);

        // Update workspace members once again to get the user informations.
        state_add_one_time_flags(STATE_FORCE_SYNC | STATE_WANT_MEMBERS);

        // Request OK.
        this.reqFinishOK();
    }
    catch(err)
    {
        THIS.error(func_name + "() error: '" + err + "'.");
        this.reqFinishErr('Internal error.');
    }
};

// Callback: identity was not set.
PubWS.prototype._cbIdentitySelectionFailure = function(transport)
{
    var func_name = "_cbIdentitySelectionFailure";
    try
    {
        var error_msg = 'An error occured while setting the chosen identity on the server.';
        if (transport.getHeader('X-JSON-Exception'))
        {
            var res_dict = transport.responseText.evalJSON(sanitize = true);
            if (res_dict['type'].search('KAjaxViewException') != -1)
            {
                error_msg = res_dict['exception'];
            }
        }

        this.reqFinishErr(error_msg);
    }
    catch(err)
    {
        this.error(func_name + "() error: '" + err + "'.");
        this.reqFinishErr('Internal error.');
    }
};

// Finish a successful request.
PubWS.prototype.reqFinishOK = function()
{
    // Set status to ready.
    this._status = 'ready';

    this.debug(9, "Request finishing...");

    try { this._module_success_callback(); }
    catch(err) { this.error("reqFinishOK() error: '" + err + "'."); }

    this.debug(8, "Request finished.");
};

// Finish a failed request and show an error message.
PubWS.prototype.reqFinishErr = function(msg)
{
    this.debug(9, "Request finishing with error: '" + msg + "'.");

    try { this._module_failure_callback(msg); }
    catch(err) { this.error("reqFinishErr error: '" + err + "'."); }

    this._status = 'idle';

    this.debug(8, "Request finished with error.");

};

// Cancel request.
PubWS.prototype.reqCancel = function()
{
    this.debug(9, "Request cancelling...");

    try { this._module_cancel_callback(); }
    catch(err) { this.error("reqCancel() error: '" + err + "'."); }

    this._status = 'idle';

    this.debug(8, "Request cancelled.");
};

// Show status bar.
PubWS.prototype.showStatusProgress = function(deactivate)
{
    var THIS = this;

    Modalbox.hide_afterLoad = false;

    Modalbox.show('<div id="progressbar_container"></div>',
                  {
                      title:'Please wait...',
                      width:400,
                      overlayClose:false,
                      onDialogClosed: function() { if($('progressbar_container')) { THIS.statusDialogClosed(); } },
                      afterLoad: function()
                      {
                          new ProgressBar(
                              'progressbar_container',
                              { classProgressBar: 'progressBar1', color: {r: 84, g: 118, b: 132} });
                          if (Modalbox.hide_afterLoad && $('progressbar_container'))
                          {
                               Modalbox.hide();
                               Modalbox.hide_afterLoad=false;
                          }
                      }
                  });

    if (deactivate)
        Modalbox.deactivate();
};

// Hide status bar.
PubWS.prototype.hideStatusProgress = function()
{
    if (this._hide_progress_bar && Modalbox.initialized)
    {
        if ($('progressbar_container')) { Modalbox.hide(); }
        else { Modalbox.hide_afterLoad = true; }
    }
};

// Callback: status dialog was closed.
PubWS.prototype.statusDialogClosed = function() { this.reqCancel(); }




/* PubWS module base object */

var PubWSModule = function()
{
    this._name = 'no_name';
    this._status_perm_check = 'pubws.req.dummy';
    this._status = 'idle';
    this._req_timeout_seconds = 0;
    this._pubws = null;
    this._pubws_link_ids = new Array('pubws_link_ids');
    this._pubws_nolink_ids = new Array('pubws_nolink_ids');
    this._links_enabled = false;
    this._single_use = true;
    this._int_req_id = 0;
    this._hide_progress_bar = true;
    this._url_path = '';
};

// Log an error using DebugConsole.
PubWSModule.prototype.error = function(msg) { DebugConsole.error(this._name, msg); };

// Log debug information using DebugConsole.
PubWSModule.prototype.debug = function(level, msg) { DebugConsole.debug(level, this._name, msg); };

// Init object.
PubWSModule.prototype.init = function(pubws) 
{ 
    this._pubws = pubws;
    this.loadTemplates();
    this.initWidgets();
    this.initStateHandlers();
};

// Load templates.
PubWSModule.prototype.loadTemplates = function() { };

// Initialize widgets.
PubWSModule.prototype.initWidgets = function() { };

// Initialize state handlers.
PubWSModule.prototype.initStateHandlers = function() { };

// Set user ID.
PubWSModule.prototype.setUserID = function() { };

// Update permissions.
PubWSModule.prototype.updatePerms = function(perms) 
{
    if (perms.hasRole('freeze'))
    {
        this.reqDisable("Workspace is moderated.");
    }
    else
    {
        if (!perms.hasPerm(this._status_perm_check)) { this._status = 'ready'; }
        this.reqConditionallyEnable();
    }
};

// Handle click on module link.
PubWSModule.prototype.click = function()
{
    if (this._links_enabled)
    {
        this.request(this._int_req_id, {});
    }
    else
    {
        // FIXME Show that the link is disabled.
        this.error("Ignoring click: link(s) are disabled.");
    }

    return false;
};

// Debug stale internal request ID.
PubWSModule.prototype.reqIdStaleDebug = function(func_name, int_req_id)
{
    this.debug(1, func_name + "() warning: int_req_id stale (this=" + int_req_id + ", current=" + this._int_req_id + ")."); 
};

// Request handler
// Note: It is used as a callback in some situations.
PubWSModule.prototype.request = function(int_req_id, arg_dict)
{
    if (int_req_id && int_req_id != this._int_req_id)
    {
        this.reqIdStaleDebug('request', int_req_id);
        return;
    }

    var THIS = this;
    try
    {
        if (this._pubws.getStatus() == 'ready')
        {
            if (this._status == 'ready')
            {
                this.finalAction(arg_dict);
            }
            else if (this._status == 'pending')
            {
                // Should not happen, except for double clicks, in which case nothing should be done.
                this.debug(1, 'request(): doing nothing: status is pending.');
            }
            else
            {
                // Start a request.
                this.reqStart();

                // Do a request.
                this.ajaxRequest(arg_dict);
            }
        }
        else if (this._pubws.getStatus() == 'pending')
        {
            // Should not happen, except for double clicks, in which case nothing should be done.
            this.debug(1, 'request(): pubws status is pending.');
        }
        else
        {
            // Start PubwS identity selection.
            this._pubws.reqIdentity(
                function() { THIS.request(THIS._int_req_id, arg_dict) }, 
                function(msg) { THIS.reqFinishErr(msg, THIS._int_req_id); }, 
                function() { THIS.reqCancel(THIS._int_req_id); });
        }
    }
    catch(err)
    {
        this.error("request(): '" + err + "'.");
        this.reqFinishErr('Internal error.');
    }
};

// Do the module request.
PubWSModule.prototype.ajaxRequest = function(arg_dict)
{
    // Sample
    var url = this._url_path + '/' + this._pubws._workspace_id;
    var params = {};
    
    var THIS = this;
    this.debug(6, "ajaxRequest(): ajax url: '" + url + "'.");
    new KAjax('pubws-mod').jsonRequest(url, params, 
                            function(res_dict, transport) { THIS._cbAjaxRequestSuccess(THIS._int_req_id, arg_dict, res_dict); },
                            function(transport) { THIS._cbAjaxRequestFailure(transport, THIS._int_req_id); }, 'POST');
};

// PubWS method (callback): identity was not set.
PubWSModule.prototype._cbAjaxRequestSuccess = function(int_req_id, arg_dict, res_dict)
{
    var func_name = "_cbAjaxRequestSuccess";
    try
    {
        if (int_req_id != this._int_req_id) { this.reqIdStaleDebug(func_name, int_req_id); return; }
        
        // Module request success.
        var finished = this.ajaxRequestSuccess(arg_dict, res_dict);

        if (finished)
        {
            // Finish request.
            this.reqFinishOK();

            // Run the final action.
            this.finalAction(arg_dict);
        }
    }
    catch(err)
    {
        this.error(func_name + "() error: '" + err + "'.");
        this.reqFinishErr('Internal error.');
    }
};

// Callback: ajax request failed.
PubWSModule.prototype._cbAjaxRequestFailure = function(transport, int_req_id)
{
    var func_name = "_cbAjaxRequestFailure";
    try
    {
        if (int_req_id != this._int_req_id) { this.reqIdStaleDebug(func_name, int_req_id); return; }

        var error_msg = 'An error has occured.';
        if (transport.getHeader('X-JSON-Exception'))
        {
            var res_dict = transport.responseText.evalJSON(sanitize = true);
            if (res_dict['type'].search('KAjaxViewException') != -1)
            {
                error_msg = res_dict['exception'];
            }
        }

        this.reqFinishErr(error_msg);
    }
    catch(err)
    {
        this.error(func_name + "() error: '" + err + "'.");
        this.reqFinishErr('Internal error.');
    }

};

// Do whatever module action.
PubWSModule.prototype.finalAction = function(arg_dict)
{
    this.debug(1, "finalAction(): No-op");
};

// Do whatever needs to be done when the json request is successful.
PubWSModule.prototype.ajaxRequestSuccess = function(arg_dict, res_dict)
{
    // Request is finished.
    return true;
};

// Enable pubws link.
PubWSModule.prototype.reqConditionallyEnable = function()
{
    var THIS = this;
    if (this._status == 'ready' && this._single_use) 
    { 
        // Status is ready and this module has been used once... don't enable requests.
        this.debug(4, "reqConditionalyEnable(): module has been used once... disabling requests."); 
        this.reqDisable();
    }
    else
    {
        this.debug(9, "reqConditionallyEnable(): enabling requests...");
        this.reqEnable();
        this.debug(8, "reqConditionallyEnable(): requests enabled.");
    }
};

// Disable pubws link.
PubWSModule.prototype.reqDisable = function(reason)
{
    if (reason == undefined) { reason = 'Request disabled.'; }

    this.debug(9, "reqDisable(): disabling requests...");

    this._links_enabled = false;

    // Hide links.
    for (var i=0; i<this._pubws_link_ids.length; i++) { $(this._pubws_link_ids[i]).hide(); }

    // Show inactivated pseudo-link.
    for (var i=0; i<this._pubws_nolink_ids.length; i++) $(this._pubws_nolink_ids[i]).title = reason;
    for (var i=0; i<this._pubws_nolink_ids.length; i++) $(this._pubws_nolink_ids[i]).show();

    this.debug(8, "reqDisable(): requests disabled.");
};

// Enable pubws link.
PubWSModule.prototype.reqEnable = function(reason)
{
    var THIS = this;
    this.debug(9, "reqEnable(): enabling requests...");

    // Hide inactivated pseudo-link.
    for (var i=0; i<this._pubws_nolink_ids.length; i++) $(this._pubws_nolink_ids[i]).hide();

    // Show link.
    for (var i=0; i<this._pubws_link_ids.length; i++) $(this._pubws_link_ids[i]).show();

    // Remove the 'disabled' class from link.
    // Drop click event if needed (Element.observe adds events instead of setting them only once).
    // Note: Element.stopObserving() works even if no click event is set, which is what we want.
    for (var i=0; i<this._pubws_link_ids.length; i++) Element.stopObserving($(this._pubws_link_ids[i]).firstChild, 'click');
    
    // Add click event.
    for (var i=0; i<this._pubws_link_ids.length; i++) 
    {
        Element.observe($(this._pubws_link_ids[i]).firstChild, 'click', function() { THIS.click(); });
    }
    this._links_enabled = true;

    this.debug(8, "reqEnable(): requests enabled.");
};

// Start a request.
PubWSModule.prototype.reqStart = function()
{
    // Set status to 'pending'.
    this._status = 'pending';

   
    // Increment the internal request ID. 
    this._int_req_id++;

    this.debug(1, "Request starting...");

    // Show status progress. 
    // Note: It can already be shown.
    this.showStatusProgress();

    // Start timer.
    this.reqStartTimer();

    // Disable module.
    this.reqDisable();
};

// Start request timer.
PubWSModule.prototype.reqStartTimer = function() 
{
    if (this._req_timeout_seconds > 0) 
    { 
        // Start timer.
        var THIS = this;
        var int_req_id = this._int_req_id;
        setTimeout(function() { THIS._reqTimeout(int_req_id); }, this._req_timeout_seconds * 1000);

        this.debug(9, "Timeout timer started.");
    }
};

// Callback: internal request timeout.
PubWSModule.prototype._reqTimeout = function(int_req_id)
{
    if (int_req_id == this._int_req_id)
    {
        this.debug(7, "Timout for request '" + int_req_id + "', current request is '" + this._int_req_id + "'.");
        this.reqTimeout();
    }
    else
    {
        // Timer from a former old request... ignore.
        this.debug(5, "Ignoring timout for request '" + int_req_id + "', current request is '" + this._int_req_id + "'.");
    }
};

// Callback: request timeout.
PubWSModule.prototype.reqTimeout = function() 
{ 
    this.reqFinishErr("Request timeout: please try again later.");
};

// Finish a successful request.
// Note: It can be used as a callback in some situations. It then needs a valid int_req_id.
// Don't try-catch even if it can be used as a callback because callback errors are already logged by KAJax.
PubWSModule.prototype.reqFinishOK = function(int_req_id)
{
    // (Only needed when this is used as a callback. Wrapper callbacks could have been used instead..
    if (int_req_id && int_req_id != this._int_req_id) { this.reqIdStaleDebug('reqFinishOK', int_req_id); return; }

    // Increment the internal request ID.                                             
    this._int_req_id++;

    this.deinitStateRequest();
    this.reqConditionallyEnable();

    // Set status to ready.
    this._status = 'ready';

    this.debug(9, "Request finishing...");

    this.hideStatusProgress();

    this.debug(8, "Request finished.");
};

// Finish a failed request and show an error message.
// Note: It can be used as a callback in some situations. It then needs a valid int_req_id.
// Don't try-catch even if it can be used as a callback because callback errors are already logged by KAJax.
PubWSModule.prototype.reqFinishErr = function(msg, int_req_id)
{
    // (Only needed when this is used as a callback. Wrapper callbacks could have been used instead..
    if (int_req_id && int_req_id != this._int_req_id) { this.reqIdStaleDebug('reqFinishErr', int_req_id); return; }

    // Increment the internal request ID.                                             
    this._int_req_id++;

    this.debug(9, "Request finishing with error: '" + msg + "'.");

    // Don't hide status progress here, because the modal box will be reused by dialog_error.
    //this.hideStatusProgress();

    dialog_error(null, msg);

    this.deinitStateRequest();
    this.reqConditionallyEnable();

    this._status = 'idle';

    this.debug(8, "Request finished with error.");
};

// Cancel request.
// Note: It can be used as a callback in some situations. It then needs a valid int_req_id.
// Don't try-catch even if it can be used as a callback because callback errors are already logged by KAJax.
PubWSModule.prototype.reqCancel = function(int_req_id)
{
    // (Only needed when this is used as a callback. Wrapper callbacks could have been used instead..
    if (int_req_id && int_req_id != this._int_req_id) { this.reqIdStaleDebug('reqCancel', int_req_id); return; }

    // Increment the internal request ID.                                             
    this._int_req_id++;

    this.debug(9, "Request cancelling...");

    this.hideStatusProgress();

    this.deinitStateRequest();
    this.reqConditionallyEnable();

    this._status = 'idle';

    this.debug(8, "Request cancelled.");
};

// De-init state request parameters.
PubWSModule.prototype.deinitStateRequest = function() { };

// Show status bar.
PubWSModule.prototype.showStatusProgress = PubWS.prototype.showStatusProgress;

// Hide status bar.
PubWSModule.prototype.hideStatusProgress = PubWS.prototype.hideStatusProgress;

// Callback: status dialog was closed.
PubWSModule.prototype.statusDialogClosed = PubWS.prototype.statusDialogClosed;







/* PubWS chat module */

var PubWSChat = function()
{
    this._name = 'PubWSChat';
    this._status_perm_check = 'pubws.req.chat';
    this._status = 'idle';
    this._req_timeout_seconds = 0; // Timeout is handled by other functions.
    this._pubws = null;
    this._pubws_link_ids = new Array('chat_link', 'att_chat_link');
    this._pubws_nolink_ids = new Array('chat_nolink', 'att_chat_nolink');
    this._links_enabled = false;
    this._single_use = true;
    this._int_req_id = 0;
    this._hide_progress_bar = true;
    this._url_path = get_url_path('teambox_pubws_chat_request');
};
PubWSChat.prototype = new PubWSModule();

// Initialize widgets.
PubWSChat.prototype.initWidgets = function() 
{
    // Change the sender name in the attachment chat link.
    var att_chat = $('attachments_chat');
    att_chat.innerHTML = att_chat.innerHTML.replace(/var_sender/g, get_identity_name_or_email(this._pubws._identities[0]));
 
    // Initialize chat and chat POST widgets.
    ChatWidget.channel_id = user_id;
    ChatWidget.init();
    ChatPOSTWidget.init();
};

// Initialize state handlers.
PubWSChat.prototype.initStateHandlers = function()
{
    state_handlers[state_handlers.length] = new Array(STATE_WANT_CHAT, ChatWidget.update_messages);
};

// Set user ID.
PubWSChat.prototype.setUserID = function() { ChatWidget.channel_id = user_id; };

PubWSChat.prototype.chatRequestResultRequest = function(workspace_id, pubws_req_id, chat_req_id, req_start_time)
{
    // Ignore stale requests.
    if (pubws_req_id < this._int_req_id) { this.debug(1, "Ignoring stale chatRequestResultRequest() call."); return; }

    // Do the request.
    var THIS = this;
    var url = get_url_path('teambox_pubws_chat_request_result') + '/' + this._pubws._workspace_id + '/' + chat_req_id;
    this.debug(6, "Chat request result url: '" + url + "'.");
    var THIS = this;
    new KAjax('chat_res').jsonRequest(url, { 'req_start_time' : req_start_time },
        function(res_dict, transport) { THIS._cbChatRequestResultSuccess(res_dict, transport, THIS._int_req_id); },
        function(transport) { THIS._cbChatRequestResultFailure(transport); });
};

PubWSChat.prototype.ajaxRequestSuccess = function(arg_dict, res_dict)
{
    var func_name = 'ajaxRequestSuccess';
    this.debug(9, func_name + "(): res_dict: '" + dump(res_dict) + "'.");

    var chat_req_id = res_dict['chat_req_id'];
    this.debug(5, "Chat request '" + chat_req_id + "' is pending... waiting for result.");

    // Start another ajax request to get the result.
    var cur_time = parseInt((new Date()).getTime()/1000);
    this.chatRequestResultRequest(this._workspace_id, this._int_req_id, chat_req_id, cur_time);

    // Request is not finished.
    return false;
};

PubWSChat.prototype._cbChatRequestResultSuccess = function(res_dict, transport, int_req_id)
{
    var chat_request_result_delay = 5;    // Delay between requests.
    var chat_request_result_timeout = 65; // A little more than the real timeout kwmo-kwm...
                                          // Must keep in sync!

    var THIS = this;

    try
    {
        res = res_dict;

        if (res['result'] == 'pending')
        {
            this.debug(6, "Chat request pending...");
            var chat_req_id = res['chat_req_id'];
            var req_start_time = parseInt(res['req_start_time']);
            var cur_time = parseInt((new Date()).getTime()/1000);
            if (cur_time > req_start_time + chat_request_result_timeout)
            {
                this.error('_cbChatRequestResultSuccess() error: chat request timeout.');
                this.reqFinishErr('Chat request timeout.');
            }
            else
            {
                // Do next request.
                setTimeout(
                    function() 
                    { 
                        THIS.chatRequestResultRequest(THIS._workspace_id, int_req_id, 
                            chat_req_id, req_start_time);
                    }, 
                    chat_request_result_delay * 1000);
            }
        }
        else if (res['result'] == 'ok')
        {
            this.reqFinishOK();
            this.debug(1, "Chat request accepted... chat widget will be activated when permissions are sync.");
        }
        else
        {
            this.error('_cbChatRequestResultSuccess() error: bad chat request result.');
            this.reqFinishErr('An internal error has occured.');
        }
    }
    catch(err)
    {
        this.error("_cbChatRequestResultSuccess() error: '" + err + "'.");
        this.reqFinishErr('An internal error has occured.');
    }
};

// Callback: ajax chat request result failure.
PubWSChat.prototype._cbChatRequestResultFailure = function(transport)
{
    var func_name = "_cbChatRequestResultFailure";
    try
    {
        var error_msg = 'An error has occured while getting chat request result.';
        if (transport.getHeader('X-JSON-Exception'))
        {
            var res_dict = transport.responseText.evalJSON(sanitize = true);
            if (res_dict['type'].search('KAjaxViewException') != -1)
            {           
                error_msg = res_dict['exception'];
            }
        }

        this.reqFinishErr(error_msg);
    }
    catch(err)
    {
        this.error(func_name + "() error: '" + err + "'.");
        this.reqFinishErr('Internal error.');
    }
};






/* PubWS KFS upload module */

var PubWSKFSUp = function()
{
    this._name = 'PubWSKFSUp';
    this._status_perm_check = 'pubws.req.kfsup';
    this._status = 'idle';
    this._req_timeout_seconds = 15;
    this._pubws = null;
    this._pubws_link_ids = new Array('send_file_link');
    this._pubws_nolink_ids = new Array('send_file_nolink');
    this._links_enabled = false;
    this._single_use = false;
    this._int_req_id = 0;
    this._hide_progress_bar = true;
    this._url_path = get_url_path('teambox_pubws_kfsup_request');
};
PubWSKFSUp.prototype = new PubWSModule();

// Callback: status dialog was closed.
PubWSKFSUp.prototype.statusDialogClosed = function() 
{
    try { thf.terminate(); }
    catch(err) { this.error("Could not terminate: "+err); } 
    PubWSModule.prototype.statusDialogClosed.call(this);
}

// Load templates.
PubWSKFSUp.prototype.loadTemplates = function() { dom_templates_extract('file_upload_template', 'file_upload'); };

// Update permissions.
PubWSKFSUp.prototype.updatePerms = function(perms)
{
    if (perms.hasRole('freeze'))
    {
        this.hideFileUploadWidget();
        this.reqDisable("Workspace is moderated.");
    }
    else
    {
        if (!perms.hasPerm(this._status_perm_check)) { this._status = 'ready'; }
        this.reqConditionallyEnable();
    }
};

// Enable pubws link.
PubWSKFSUp.prototype.reqConditionallyEnable = function()
{
    if (this._pubws && this._pubws._misc_info && this._pubws._misc_info['att_expire_flag'] == 1)
    {
        // Attachments have expired. Don't allow dropping back files.
        this.debug(4, "reqConditionalyEnable(): attachments have expired... disable file drop-back.");
        this.reqDisable();
    }
    else
    {
        // Super
        PubWSModule.prototype.reqConditionallyEnable.call(this);
    }
};

// Disable upload.
PubWSKFSUp.prototype.reqDisable = function(reason)
{
    // Hide the file upload widget, if needed.
    this.hideFileUploadWidget();

    // Super
    PubWSModule.prototype.reqDisable.call(this, reason);
};

PubWSKFSUp.prototype.ajaxRequestSuccess = function(arg_dict, res_dict)
{
    var func_name = 'ajaxRequestSuccess';
    this.debug(9, func_name + "(): res_dict: '" + dump(res_dict) + "'.");

    /*
    if (arg_dict['result'] == 'timeout') 
    { 
        this._reqFinishErr('Server timeout... please try again later.'); 
    }
    */
    
    // Request is finished.
    return true;
};


// Do whatever module action.
PubWSKFSUp.prototype.finalAction = function(arg_dict)
{
    this.debug(4, "Enabling file upload.");

    var THIS = this;
    var obj, obj_parent, elem;

    // Get the container.
    obj_parent = get_by_id("file_upload_container");

    // Remove former form if needed.
    obj  = get_by_id('file_upload');
    if (obj) { obj_parent.removeChild(obj); }

    // Get the form template.
    obj = dom_templates_get('file_upload');
    Element.extend(obj);
    obj.show();

    // Add template form in container.
    obj_parent.appendChild(obj);

    // Set the submit event function.
    var form = $('form_upload');
    set_event_by_obj(form, 'submit', function() { return THIS.formUploadSubmit(form); });
};

// Hide file upload widget.
PubWSKFSUp.prototype.hideFileUploadWidget = function()
{
    var obj  = $('file_upload');
    if (obj) { obj.hide() ;}
};

// This function handles the start of an upload.
PubWSKFSUp.prototype.uploadStart = function()
{
    this.debug(1, "Starting file upload.");
    this.hideFileUploadWidget();
    this.showStatusProgress();
    return true;
};

// This function handles the onSubmit event of the upload file form.
PubWSKFSUp.prototype.formUploadSubmit = function(form)
{
    if($('file_upload_input').value.empty())
    {
        this.hideFileUploadWidget();
        return false;
    }
    else
    {
        form = get_by_id("form_upload");
        form.action = get_url_path('teambox_upload') + '/' + this._pubws._workspace_id;
        this.debug(5, "File upload prepared.");

        var THIS = this;
        thf.prepare(form, null, 
                 function() { return THIS.uploadStart(); },
                 function(iframe_output) { return THIS.fileUploadComplete(iframe_output); });
        return thf.submit();
    }
};

// Callback: file upload is complete.
PubWSKFSUp.prototype.fileUploadComplete = function(iframe_output)
{
    try
    {
        // Get json status from the iframe output.
        // (Some?) browsers surround it with <pre></pre>... remove it.
        var json_str = iframe_output.replace(/^<pre>/gi, "").replace(/<\/pre>$/gi, "");

        var status = json_str.evalJSON(sanitize = true);
        if (status["result"] == "ok")
        {
            this.debug(1, "File upload complete.");
            this.hideStatusProgress();
            GlobalMessage.info("File upload is complete.");
        }
        else if (status['result'] == "error")
        {
            this.error("File upload failed: " + status['error'] + ".");
            this.hideStatusProgress();
            GlobalMessage.error("File upload failed: " + status['error'] + ".");
        }
        else
        {
            this.error("File upload failed.");
            this.hideStatusProgress();
            GlobalMessage.error("File upload failed.");
        }
    }
    catch(err)
    {
        this.error("fileUploadComplete() error: '" + err + "'.");
        try { dialog_error(null, "Unexpected file upload error."); }
        catch(e) { }
    }
};






/* PubWS KFS download module */
var PubWSKFSDown = function()
{
    this._name = 'PubWSKFSDown';
    this._status_perm_check = 'pubws.req.kfsdown';
    this._status = 'idle';
    this._req_timeout_seconds = 10;
    this._pubws = null;
    this._pubws_link_ids = null; // Not used
    this._pubws_nolink_ids = null; // Not used
    this._links_enabled = false;
    this._single_use = false;
    this._int_req_id = 0;
    this._hide_progress_bar = true;

    this._container_node_id = 'kfs_view';
    this._files = new Array();
    this._url_path = get_url_path('teambox_pubws_kfsdown_request');
};
PubWSKFSDown.prototype = new PubWSModule();

// Initialize state handlers.
PubWSKFSDown.prototype.initStateHandlers = function()
{
    var THIS = this;
    state_handlers[state_handlers.length] = new Array(STATE_WANT_KFS, function(result) { THIS.kfsUpdateView(result); });
    state_add_cur_flags(STATE_WANT_KFS);
};

// Update permissions.
PubWSKFSDown.prototype.updatePerms = function(perms)
{
    if (!perms.hasPerm(this._status_perm_check)) { this._status = 'ready'; }
    // Don't disable request when workspace is frozen (this is a read operation).
    this.reqConditionallyEnable();
};

// Enable requests.
PubWSKFSDown.prototype.reqConditionallyEnable = function()
{
    this._links_enabled = true;
    this.debug(8, "Links enabled (no-op).");
};

// Disable requests.
PubWSKFSDown.prototype.reqDisable = function(reason)
{
    this._links_enabled = false;
    this.debug(8, "Links disabled (no-op).");
};

// Handle click on the workspace creation pubws link.
PubWSKFSDown.prototype.click = function(identity_email, node_idx)
{
    if (this._links_enabled)
    {
        this.request(this._int_req_id, {identity_email : identity_email, node_idx : node_idx});
    }
    else
    {
        // FIXME Show that the link is disabled.
        this.error("Ignoring click: link(s) are disabled.");
    }

    return false;
};

// Do whatever module action.
PubWSKFSDown.prototype.finalAction = function(arg_dict)
{
    this.debug(4, "Downloading file...");

    // Start download.
    var identity_email = arg_dict['identity_email'];
    var node_idx = arg_dict['node_idx'];
    var file = this._files[identity_email][node_idx];
    url = kfs_download_open_url(file);
    document.location.href = url;
};

// Update KFS view.
PubWSKFSDown.prototype.kfsUpdateView = function(result)
{
    var evt_id = result["last_evt"];
    var files = result['data']['user_files'];

    this.setFiles(files);
    this.refreshView();

    state_add_cur_params({"last_evt_kfs_id":evt_id});
};

// Set files.
PubWSKFSDown.prototype.setFiles = function(files)
{
    this.debug(7, "setFiles() called.");
    this._files = files;
    this.debug(6, "setFiles() finished.");
};

// Refresh view.
PubWSKFSDown.prototype.refreshView = function()
{
    this.debug(7, "refreshView() called.");
    var s;
    s = '';
    s += "Sender (" + this._pubws._identities[0].email + ') has ' + this._files['sender'].length + 'attachments.\n';
    this.debug(4, s);

    s = '';
    for (var i=0; i<this._pubws._identities.length; i++)
    {
        var identity = this._pubws._identities[i];
        s += "User " + identity.email + ' has ' + this._files[identity.email].length + 'drop-back files.\n';
    }
    this.debug(4, s);

    this.refreshAttachmentsView();
    this.refreshDroppedFilesView();

    this.debug(6, "refreshView() finished.");
};

// Refresh attachments view.
PubWSKFSDown.prototype.refreshAttachmentsView = function()
{
    this.debug(7, "refreshAttachmentView() called.");

    var att_nbr = this._pubws._email_info['attachment_nbr'];
    var att_expire_date = this._pubws._email_info['att_expire_date'];
    var att_expire_flag = this._pubws._misc_info['att_expire_flag'];
    var tstamp = Math.round(new Date().getTime() / 1000);
    var sender_dir_present = this._files['sender_dir_present'];
    var container, s, expired;

    // Show attachments top message, if needed.
    var attachments_template_expiring = $('attachments_template_expiring').innerHTML;
    var attachments_template_expired = $('attachments_template_expired').innerHTML;
    container = $('attachment_info');
    expired = 0;
    if (!sender_dir_present || att_expire_flag)
    {
        expired = 1;
        s = attachments_template_expired;
        s = s.replace('var_expire_date', format_date_regular(att_expire_date, false));
        container.innerHTML = s;
        container.show();
    }
    else if (att_expire_date && 
        (pubws_att_expire_warning == 0 || tstamp > (att_expire_date - pubws_att_expire_warning)))
    {
        s = attachments_template_expiring;
        s = s.replace('var_expire_date', format_date_regular(att_expire_date, false));
        container.innerHTML = s;
        container.show();
    }
    else
    {
        container.hide();
        container.innerHTML = '';
    }

    // Show attachments.
    container = $('attachment_list');
    s='';
    var attached_file_template = $('attached_file_template').innerHTML;
    var attached_file_template_expired = $('attached_file_template_expired').innerHTML;
    var attached_file_template_pending = $('attached_file_template_pending').innerHTML;
    var attached_file_template_deleted = $('attached_file_template_deleted').innerHTML;
    var file;
    var ts;
    var uploaded_files = 0;
    var deleted_files = 0;
    var pending_files = false;
    for(var i=0; i<this._files['sender'].length; i++)
    {
        file = this._files['sender'][i];
        if (file.status == 0) 
        { 
            ts = attached_file_template_pending; 
            pending_files = true;
        }
        else if (file.status == 1)
        {
            uploaded_files++;
            ts = attached_file_template; 
 
            ts = ts.replace('var_file_email', 'sender'); 
            ts = ts.replace('var_file_idx', i); 
            ts = ts.replace('var_file_size', kfs_convert_file_size(file.file_size));
        }
        else if (file.status == 2) 
        {
            deleted_files++;
            uploaded_files++;
            if (att_expire_flag == 1)
            {
                ts = attached_file_template_expired;
            }
            else
            {
                ts = attached_file_template_deleted;
            }
        }
   
        ts = ts.replace('var_file_name', file.esc_name);
   
        s += ts; 
    }
    if (sender_dir_present && !expired 
        && (pending_files == true || att_nbr > uploaded_files)) 
    { 
        s += ($('attachment_list_pending_template').innerHTML); 
    }
    if (!sender_dir_present || expired || att_nbr == deleted_files)
    {
        $('attachments_chat').show();
    }
    else
    {
        $('attachments_chat').hide();
    } 
    // Update container.
    container.innerHTML = s;
    if(this._files['sender'].length > 0 || att_nbr > 0) { $('attachments_block').show(); }

    this.debug(6, "refreshAttachmentView(): finished.");
};

// Refresh dropped files view.
PubWSKFSDown.prototype.refreshDroppedFilesView = function()
{
    this.debug(7, "refreshDroppedFilesView() called.");

    var tstamp = Math.round(new Date().getTime() / 1000);
    var att_expire_date = this._pubws._email_info['att_expire_date'];
    var att_expire_flag = this._pubws._misc_info['att_expire_flag'];

    var container;
    var identity;
    for (var j=0; j<this._pubws._identities.length; j++)
    {
        identity = this._pubws._identities[j];
        container = $("file_list_"+j);
        var s='';
        var attached_file_template = $('attached_file_template').innerHTML;
        var attached_file_template_expired = $('attached_file_template_expired').innerHTML;
        var attached_file_template_deleted = $('attached_file_template_deleted').innerHTML;
        var file;
        var ts='';
        var ready_files_count = 0;
        for(var i=0; i<this._files[identity.email].length; i++)
        {
            file = this._files[identity.email][i];

            if (file.status == 1)
            {
                ts = attached_file_template; 
                ts = ts.replace('var_file_email', identity.email); 
                ts = ts.replace('var_file_idx', i); 
                ts = ts.replace('var_file_size', kfs_convert_file_size(file.file_size));
                ready_files_count++;
            }
            else if (file.status == 2)
            {
                if (att_expire_flag == 1)
                {
                    ts = attached_file_template_expired;
                }
                else
                {
                    continue;
                }
                ready_files_count++;
            }
            ts = ts.replace('var_file_name', file.esc_name);
            s += ts;
        }
        
        // Update container.
        container.innerHTML = s;
        
        if (ready_files_count>0)
            ($('view_files_'+j)).show();
        else
            ($('view_files_'+j)).hide();
    }

    this.debug(6, "refreshDroppedFilesView() finished.");
};







/* PubWS workspace creation module */

var PubWSCreate = function()
{
    this._name = 'PubWSCreate';
    this._status_perm_check = 'pubws.req.wscreate';
    this._status = 'idle';
    this._req_timeout_seconds = 10;
    this._pubws = null;
    this._pubws_link_ids = new Array('wscreate_link');
    this._pubws_nolink_ids = new Array('wscreate_nolink');
    this._links_enabled = false;
    this._single_use = true;
    this._int_req_id = 0;
    this._hide_progress_bar = true;
    this._url_path = get_url_path('teambox_pubws_create_request');
};
PubWSCreate.prototype = new PubWSModule();


// Show a dialog box to the user stating that the request was successfully sent.
PubWSCreate.prototype.ajaxRequestSuccess = function(arg_dict, res_dict)
{
    // Workaround for modalbox hiding after the dialog is called, even though the hide() call was done before.
    this._hide_progress_bar = false;
    dialog_status(null,
        "Your request was sent to "
        + get_identity_name_or_email(this._pubws._identities[0])
        + ". You will receive an email once it has been accepted.");

    // Request is finished.
    return true;    
};
