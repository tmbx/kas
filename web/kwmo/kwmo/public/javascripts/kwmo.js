// Delays available for update state requests.
var STATE_DELAY_NONE = 0;
var STATE_DELAY_NORMAL = 2000;
var STATE_DELAY_LONG = 5000;

// Options that can be used when restarting the update state look.
var STATE_FORCE_SYNC = 1<<0;
var STATE_WANT_MEMBERS = 1<<1;
var STATE_WANT_KFS = 1<<2;
var STATE_WANT_CHAT = 1<<3;
var STATE_WANT_VNC = 1<<4;
var STATE_WANT_PERMS = 1<<6;
var STATE_WANT_WS = 1<<7;
var STATE_WANT_UPLOAD_STATUS = 1<<8;
var STATE_WANT_PUBWS_INFO = 1<<9;

// Misc
var perms = null;
var kws_id = null;
var user_id = null;
var kfs_share_id = 0;

// Resettable global variables
var state_version = 1;
var state_req_id = 0;
var state_cur_flags;
var state_cur_params;
var state_one_time_flags;
var state_one_time_params;
var members_list;
var state_timer;

// default flags and parameters - can be overriden in other scripts (ie: ws.js, pubws.js)
var state_def_flags = 0;
var state_def_params = new Object();

// Default state update handlers
state_handlers = new Array();

// Get url by name.
function get_url_path(name)
{
    // This variable is defined in the main html page.
    return kwmo_urls[name];
}

// This function initializes kwmo.
function init_kwmo()
{
    DebugConsole.viewInit('debug_console');
    DebugConsole.setDebugLevel(6);
    //DebugConsole.show();

    kws_id = null;
    obj = get_by_id("kws_id");
    if (obj && obj.value !== "") { kws_id = obj.value; }
    user_id = null;
    obj = get_by_id("user_id");
    if (obj && obj.value !== "") { user_id = obj.value; }

    document['onkeydown'] = keydown;
}

// This function updates data (updater data in the HTML code),
// and starts the updater.
function state_update_from_html_input()
{
    DebugConsole.debug(9, null, "state_update_from_html_input() called.");

    try
    {
        var res_dict = $('updater_state').value.evalJSON(sanitize = true);
        handle_updater_state(res_dict);
    }
    catch(err)
    {
        DebugConsole.error(null, 
            "state_update_from_html_input(): error handling update from html: '" + err + "'.");
    }

    try
    { 
        kwmo_first_update();
    }
    catch(err)
    {
        DebugConsole.error(null, "state_update_from_html_input(): error in kwmo_first_update: '" + err + "'.");
    }
}

function handle_enter_key(myfield, e, handlerFunc)
{
    var keycode;
    if (window.event) keycode = window.event.keyCode;
    else if (e) keycode = e.which;
    else return true;
    
    if (keycode == 13)
    {
        //typically the html element that fired the event should be passed to the handler function.
        handlerFunc();
        return false;
    }
    else
        return true;
}

// This function adds keyboard shortcuts.
function keydown(e)
{
    var evt = e || window.event;
    keycode = evt.charCode || evt.keyCode;

    // Show the debug console.
    if (keycode == 80 && evt.shiftKey && evt.altKey && evt.ctrlKey) { DebugConsole.toggle(); }

    return true;
}

// This function resets the kwmo state.
function reset_kwmo_state()
{
    // current flags and parameters - merged temporarily with default at each request
    state_reset_cur_all();

    // one-time flags and parameters - merged temporarily with current for the next request, and is reset after that
    state_reset_one_time_all();

    members_list = new Object();
    state_timer = null;
}

// This function gets next request flags.
function state_get_flags()
{
    // Merge flags.
    var flags = state_def_flags | state_cur_flags | state_one_time_flags;

    return flags;
}

// This function gets next state parameters.
function state_get_params()
{
    var key;
    var params = new Object();

    // Merge default parameters.
    for (key in state_def_params)
    {
        params[key] = state_def_params[key];
    }

    // Merge current parameters.
    for (key in state_cur_params)
    {
        params[key] = state_cur_params[key];
    }

    // Merge one-time parameters.
    for (key in state_one_time_params)
    {
        params[key] = state_one_time_params[key];
    }

    return params;
}

// This function resets one-time state flags and parameters.
function state_reset_one_time_all()
{
    state_one_time_flags = 0;
    state_one_time_params = new Object();
}

// This function resets current state flags and parameters.
function state_reset_cur_all()
{
    state_cur_flags = 0;
    state_cur_params = new Object();
}

// This function sets one-time state flags.
function state_set_one_time_flags(flags) { state_one_time_flags = flags; }

// This function adds one-time state flags.
function state_add_one_time_flags(flags) { state_one_time_flags = state_one_time_flags | flags; }

// This function sets current state flags.
function state_set_cur_flags(flags) { state_cur_flags = flags; }

// This function adds current state flags.
function state_add_cur_flags(flags) { state_cur_flags = state_cur_flags | flags; }

// This function sets one-time state parameters.
function state_set_one_time_params(params) { state_one_time_params = params; }

// This function adds one-time state parameters.
function state_add_one_time_params(params) { for (var key in params) { state_one_time_params[key] = params[key]; } }

// This function adds current state parameters.
function state_add_cur_params(params) { for (var key in params) { state_cur_params[key] = params[key]; } }

// This function removes state flags.
function state_del_cur_flags(flags) { state_cur_flags = state_cur_flags & ~flags; }

// This function removes current state parameters.
function state_del_cur_params(keys) { for (var key in keys) { delete state_cur_params[keys[key]]; } }

// This function returns a padded string.
// Parameters:   s: string to pad | c: character to use when padding | nb: total length of padded string
function lpad(s, c, nb)
{
    s = "" + s;
    len = s.length;
    p = '';
    if (nb > len) { for (var i=0; i<(nb - len); i++) { p = p + c; } }
    return p + s;
}

// This function replaces a white space for '&nbsp;' in string.
function replace_nbsp(s) { return s.replace(/ /, '&nbsp;'); }

// Log a string on server.
function server_log(s)
{
    // Send the request.
    var url = get_url_path('teambox_serverlog') + "/" + kws_id;
    DebugConsole.debug(6, null, "Server log url: '" + url + "'.");
    new KAjax('server_log').jsonRequest(url, { 'message' : s },
        server_log_success, handle_kajax_exception, 'POST');
}

// Callback: server log success
function server_log_success(res_dict, transport)
{
    DebugConsole.debug(7, null, "Server log success."); 
}

// Show freeze notice.
function show_freeze()
{
}

function hide_freeze()
{
}

// This function updates the members list.
function update_perms(res)
{
    DebugConsole.debug(7, null, "update_perms() called.");

    // Update global perms object.
    var tmp_perms = new KWMOPermissions();
    tmp_perms.from_dict(res);
    perms = tmp_perms;
    state_add_cur_params({'last_perms_update_version' : perms.update_version});

    //DebugConsole.debug(5, null, "update_perms() perms dump: " + dump(perms._rules));

    // Show-hide freeze notice.
    if (perms.hasRole('freeze')) { show_freeze(); }
    else { hide_freeze(); }

    // Handle chat post permissions.
    ChatWidget.update_perms(perms);
    ChatPOSTWidget.update_perms(perms);

    DebugConsole.debug(5, null, "update_perms() finished: object version: '"
        + perms.object_version
        + "', update_version: '" + perms.update_version + "'.");
}

// Update workspace informations.
function update_ws(result)
{
    DebugConsole.debug(7, null, "update_ws() called.");

    var evt_id = result['last_evt']

    // Update workspace name.
    document.title = 'Teambox: ' + result['data']['name'];
    if ($('workspace_name')) { $('workspace_name').innerHTML = result['data']['name']; }

    var frozen = false;
    if (result['data']['frozen'] || result['data']['deep_frozen']) { frozen = true; }

    state_add_cur_params({"last_evt_ws_id":evt_id});

    DebugConsole.debug(5, null, "update_ws() called.");
}

// Debug an async request ajax exception.
function handle_kajax_exception(transport)
{
    if (transport.getHeader('X-JSON-Exception'))
    {
        var res_dict = transport.responseText.evalJSON(sanitize = true);
        if (res_dict['type'].search('KAjaxViewException') != -1)
        {
            show_non_modal_dialog(res_dict['exception'], null);
        }
    }
}

// This function returns the number of items in an object.
function object_items_count(a)
{
    var count = 0;
    for (var key in a) { count++; }
    return count;
}

// This function merges two objects.
function object_merge(a, b)
{
    var c = new Object();
    for (var key in a)
    {
        c[key] = a[key];
    }
    for (var key in b)
    {
        c[key] = b[key];
    }
    return c;
}

// This function does an update state request.
function state_request()
{
    // Remove previous timer object.
    state_timer = null;

    // Get current request flags and parameters.
    var flags = state_get_flags();
    var params = state_get_params();

    DebugConsole.debug(9, null, "state_request(): flags: " + 
        dump(flags) + "     params: " + dump(params));

    if (flags > 0)
    {
        DebugConsole.debug(9, null, "Doing a state request with flags '" + flags + "'.");

        // Do the request now.
        // (one-time flags and parameters will be reset only in the success handler)
        var req_dict = { version: state_version, req_id:++state_req_id, req_flags:flags, req_params:params };
        var req_json = Object.toJSON(req_dict);
        
        /* Unlike Rails, Pylons doesn't know how to interpret nested paramters to build the request.params dictionary correctly, so the best way is to jsonify/dejosinfy the nested get params.
         * However, do not just encode the JSON string and append it to the url, instead, pass it as the 'parameters' argument of Ajax.request, 
         *  which will call the smart Object.toQueryString and will encode it appropriately.
         */
 
        // Send the request.
        var url = get_url_path('teambox_updater') + "/" + kws_id;
        DebugConsole.debug(7, null, "State request url: '" + url + "'.");
        new KAjax('updater').jsonRequest(url, {state_request:req_json},
            state_request_success, state_request_failure);
    }
    else
    {
        DebugConsole.debug(9, null, "Postponing state request.");
        // Postpone the request.
        state_restart_loop();
    }
}

// This function updates the members list object.
// (Converts a list to an object mapping user ids and users.
function update_global_members_list_object(list)
{
    for (var i=0; i<list.length; i++)
    {
        members_list[list[i].id] = list[i];
    }
}

// This function updates the members list.
function update_members(res)
{
    var evt_id = res["last_evt"];
    var list = res["data"];
    var user;
    var str;
    var name;

    DebugConsole.debug(5, null, "update_members(): " + list.length + " members");

    // Update global members list.
    update_global_members_list_object(list);
    
    var templateHtml = get_by_id("member_template_container").innerHTML; 
  
    if (! perms.hasRole('roadmin'))
    {
        // This is not an admin session... overwrite user.
        get_by_id("span_session_user").innerHTML = get_user_or_email(user_id);
    }
    
    // Update members list widget.
    //TODO: replace this with JST
    if (templateHtml)
    {
        str = '';
        for (var i=1; i<list.length; i++)
        {
            var user = list[i];
            var tmpStr = templateHtml;
            var status_class = '';
         
            if (user.admin_name) { name = user.admin_name; }
            else if (user.real_name) { name = user.real_name; }
            else if (user.email) { name = user.email.substr(0,user.email.indexOf('@')); }
            else { name = "[Unknown User]"; }
            tmpStr = tmpStr.replace("var_user_name", name);
            tmpStr = tmpStr.replace(/var_user_email/g, (user.email? user.email:"@"));
            if (user.locked) { status_class += ' locked'; }
            if (user.banned) { status_class += ' banned'; }
            tmpStr = tmpStr.replace(/var_user_status_class/g, status_class);
            str +=tmpStr;
        }

        var obj = get_by_id("members_list");
        if (obj)
        { 
            obj.innerHTML = str;
        }
    }

    // (only in pubws mode)
    obj = get_by_id("pubws_user_info");
    if (obj)
    {
        var s = "This is the Teambox of: " + get_user_or_email(1);
        obj.innerHTML = s;
        text_nodes_map(null, text_nodes_replace, 
            {"search":"{owner}", "replace":get_user_or_email(1, false)});
        show_by_id("header_owner")
    }
    state_add_cur_params({"last_evt_user_id":evt_id});
}

// This function converts a unix timestamp to this format: 'DD-MM-YYYY HH:SS'.
function format_date(stamp)
{
    var dt = new Date(stamp * 1000);
    var year = dt.getYear();
    if (year < 2000) { year += 1900; } // inconsistency with IE6
    return lpad(dt.getDate(), 0, 2) + '-' + lpad(dt.getMonth()+1, 0, 2) + '-' + year
        + '&nbsp;' + lpad(dt.getHours(), 0, 2) + ":" + lpad(dt.getMinutes(), 0, 2);
}

// This function converts a unix timestamp to the ISO date format.
function format_date_iso(stamp, want_seconds)
{
    var dt = new Date(stamp * 1000);
    var year = dt.getYear();
    if (year < 2000) { year += 1900; } // inconsistency with IE6
    var s = year + '-' + lpad(dt.getMonth()+1, 0, 2) + '-' + lpad(dt.getDate(), 0, 2)
        + '&nbsp;' + lpad(dt.getHours(), 0, 2) + ":" + lpad(dt.getMinutes(), 0, 2);
    if (want_seconds) { s = s + ":" + lpad(dt.getSeconds(), 0, 2); }
    return s;
}

// This function converts a unix timestamp to a regular date.
function format_date_regular(stamp)
{
    var dt = new Date(stamp * 1000);
    var year = dt.getYear();
    var months = new Array("January", "February", "March", "April", "May", "June", "July",
                           "August", "September", "October", "November", "December");
    var weekdays = new Array("Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday");

    if (year < 2000) { year += 1900; } // inconsistency with IE6
    var s = weekdays[dt.getDay()] + ', ' + lpad(dt.getDate(), 0, 2) + ' ' + months[dt.getMonth()] + ' ' + year;
    return s;
}


/*
function format_date_or_time(stamp)
{
    var dtnow = new Date();
    var dt = new Date(stamp * 1000);
    var year = dt.getYear();
    if (year < 2000) { year += 1900; } // inconsistency with IE6
    if (dt.getYear() == gtnow.getYear() && dt.getYear() == gtnow.getYear() && 
        dt.getYear() == gtnow.getYear())
    {
        return lpad(dt.getHours(), 0, 2) + ":" + lpad(dt.getMinutes(), 0, 2) + 
            ":" + lpad(dt.getSeconds(), 0, 2);
    }
    else
    {
        return year + "-" + lpad(dt.getMonth()+1, 0, 2) +    "-" + lpad(dt.getDate(), 0, 2)
    }
}
*/

// This function returns a user email using it's ID.
function get_user_email(id)
{
    var user;
    user = members_list[id];
    if (user) { return user.email; }
    return "unknown";
}

// This function returns a user name (if present) or it's email using it's ID.
function get_user_or_email(id, nbsp)
{
    if (nbsp == null) { nbsp = true; }
    var user;
    user = members_list[id];
    if (user)
    {
        if (user.admin_name) { return (nbsp ? replace_nbsp(user.admin_name) : user.admin_name); }
        if (user.real_name) { return (nbsp ? replace_nbsp(user.real_name) : user.real_name); }
        return user.email;
    }
    return "Guest";
}

// This function returns a string containing a user name and email using it's ID.
function get_user_name_and_email(id)
{
    var user;
    user = members_list[id];
    if (user)
    {
        if (user.name) { return replace_nbsp(user.name + ' (' + user.email + ')'); }
        else { return user.email; }
    }
    return "unknown";
}



    
// Convert file size
function kfs_convert_file_size(n)
{
    var kb = 1024;
    var mb = 1024 * 1024;
    var gb = 1024 * 1024 * 1024;
    if (n > gb) { return (Math.round((n / gb) * 10) / 10) + " GB"; }
    if (n > mb) { return (Math.round((n / mb) * 10) / 10) + " MB"; }
    if (n > kb) { return (Math.round((n / kb) * 10) / 10) + " KB"; }
    return n + "  B";
}

// This function gets the download and save link for a file.
function kfs_download_save_url(kfs_file)
{
    var req_json = Object.toJSON(kfs_file)
    url = get_url_path('teambox_download') + "/" + kws_id + "?" + Object.toQueryString( {kfs_file:req_json, mode:'save'} );
    return url;
}

// This function gets the download and open link for a file.
function kfs_download_open_url(kfs_file)
{
    var req_json = Object.toJSON(kfs_file)
    url = get_url_path('teambox_download') + "/" + kws_id + "?" + Object.toQueryString( {kfs_file:req_json, mode:'open'} );
    return url;
}

// This function stops the update state loop.
// Note: state requests now use request IDs, so old request results are ignored anyways.
function state_stop_loop()
{
    // Clear current timer if any.
    if (state_timer) { try { clearTimeout(state_timer); } catch(e) { } }
}

// This function starts or restarts the update state loop.
function state_restart_loop(first_delay)
{
    DebugConsole.debug(9, null, "state_restart_loop() called.");

    // Stop loop if it is started.
    state_stop_loop();

    // set the delay for this update only
    var delay = first_delay;
    if (delay == null) { delay = STATE_DELAY_NORMAL; }

    DebugConsole.debug(9, null, "DELAY: '" + delay + "'.");

    if (delay > 0) 
    {
        // Postpone the request a bit.
        state_timer = setTimeout(state_request, delay); 
    }
    else
    {
        // Do the request now.
        state_request();
    }
    DebugConsole.debug(9, null, "state_restart_loop() finished.");
}

// This function reloads the page.

// This function handles state.
function handle_updater_state(res_dict)
{
    try
    {
        var version = res_dict['version'];
        var req_id = res_dict['req_id'];
        DebugConsole.debug(5, null, "handle_updater_state(): version='" + version + "', req_id='" + req_id + "'.");
        if (version > state_version)
        {
            show_non_modal_dialog(
                "This page will be reloaded because the server has been updated to a new version.",
                page_force_reload);
            return;
        }
        // Update global state version.
        state_version = version;
        if (req_id > 0 && req_id < state_req_id)
        {
            DebugConsole.debug(1, null, "handle_updater_state(): ignoring state update: req_id is stale.");
            return;
        }

        // Reset one-time flags and parameters.
        state_reset_one_time_all();

        var res = res_dict["state"];

        // Handle update for each "module", in order.
        for (var i=0; i<state_handlers.length; i++)
        {
            var state_handler = state_handlers[i];
            var key = state_handler[0];
            var func = state_handler[1];

            if (res && res[key])
            {
                DebugConsole.debug(7, null, "handle_updater_state(): update key " + key);

                try
                {
                    func(res[key]);
                }
                catch(e)
                {
                    DebugConsole.debug(1, null, "State request handler for key " + key + ": '" + e + "'.");
                    throw(e);
                }
            }
            else
            {
                DebugConsole.debug(12, null, "handle_updater_state(): no result for key '" + key + "'.");
            }
        }
        state_restart_loop();
    }
    catch (e)
    {
        DebugConsole.error(null, "State request handlers exception: '" + e + "'.");
        //state_restart_loop(STATE_DELAY_LONG);
    }
}

// This function (callback) handles a successful update state request.
function state_request_success(res_dict, transport)
{
    try
    {
        DebugConsole.debug(9, null, "state_request_success() called.");

        //DebugConsole.debug(9, null, "state_request_success() res_dict dump: '" + dump(res_dict) + "'");

        // Handle update.
        handle_updater_state(res_dict);

        DebugConsole.debug(9, null, "state_restart_loop() finished.");
    }
    catch(err)
    {
        DebugConsole.error(null, "state_request_success(): error: '" + err + "'.");
    }
}

// This function (callback) handles a failed update state request.
function state_request_failure(transport)
{
    try
    {
        DebugConsole.error(null, "Status update failed: status='" + transport.status + "'.");
        state_restart_loop(STATE_DELAY_LONG);
    }
    catch(err)
    {
        DebugConsole.error(null, "state_requert_failure(): error: '" + err + "'.");
    }
}

/* Show a generic message box to the user. Supports a callback when dialog is closed. */
function show_non_modal_dialog(message, close_callback)
{
    /* TODO: Replace with something better. */

    // Alert. It blocks everything until closed.
    alert(message);

    // Call the close callback.
    if (close_callback) { close_callback(); }
}

/* KWMO permissions */
function KWMOPermissions()
{
    this.object_version = null;
    this.update_version = null;
    this._rules = new Array();
    this._roles =   {
                        'root' :
                        [
                            'a:users',
                            'a:chat',
                            'a:kfs',
                            'a:vnc'
                        ],

                        'roadmin' : 
                        [
                            'a:users.list',
                            'a:chat.list',
                            'a:kfs.list',
                            'a:kfs.download',
                            'a:vnc.list'
                        ],

                        'normal' : 
                        [
                            'a:users.list',
                            'a:chat.list.channel.0',
                            'a:chat.post.channel.0',
                            'a:kfs.list.share.0',
                            'a:kfs.upload.share.0',
                            'a:kfs.download.share.0',
                            'a:vnc.list',
                            'a:vnc.connect'
                        ],

                        'skurl' : 
                        [
                            'a:users.list',
                            'a:pubws.req',
                            'a:kfs.list.share.0'
                        ],

                        'freeze' : 
                        [
                            'd:chat.post',
                            'd:kfs.upload',
                            'd:pubws.req'
                        ] 
                    };
}

/* Lookup a permission node. */
KWMOPermissions.prototype.lookup = function(node_list)
{
    var target = this._perms;
    for (var i=0; i<node_list.length; i++)
    {
        if (target[node_list[i]] != undefined) { target = target[node_list[i]]; }
        else { throw("Unexistant permission node."); }
    }
    return target;
};

/* Check for permission,
   Rules (single rules and roles) are checked in order.
   Deny rules apply immediately. */
KWMOPermissions.prototype.hasPerm = function(perm_name)
{
    if (this._ruleListCheck(this._rules, perm_name) > 0) { return true; }
    return false;
};

/* Check if role is set (currently, this checks the first level
   but does not check roles defined by other roles). */
KWMOPermissions.prototype.hasRole = function(role_name)
{
    return this._ruleRoleCheck(this._rules, role_name);
};

/* Internal: check if role is set in a set of rules (first level only). */
KWMOPermissions.prototype._ruleRoleCheck = function(rules, role_name)
{
    for (var i=0; i<rules.length; i++)
    {
        if (rules[i] == 'r:'+role_name) { return true; }
    }

    return false;
};

/* Import permissions from a dictionary. */
KWMOPermissions.prototype.from_dict = function(d)
{
    this.object_version = d['object_version'];
    this.update_version = d['update_version'];
    this._rules = d.rules;
};

/* Internal: returns permission status from a list of rules. */
KWMOPermissions.prototype._ruleListCheck = function(rules, perm_name)
{
    var res = 0, tmp_res;
    var rule, rule_arr, role_name;
    for(var i=0; i<rules.length; i++)
    {
        rule = rules[i];
        rule_arr = rule.split(':');
        rule_type = rule_arr[0];
        rule_value = rule_arr[1];

        if (rule_type == 'r')
        {
            // Role (list of rules)
            role_name = rule_value;
            tmp_res = this._roleCheck(role_name, perm_name);
        }
        else if (rule_type == 'a' || rule_type == 'd')
        {
            // Single rule
            tmp_res = this._ruleCheck(rule_type, rule_value, perm_name);
        }
        else
        {
            throw "Bad rule type: rule is: '" + rule + "'.";
        }

        // Use the result if it is authoritative only.
        if (tmp_res != 0) { res = tmp_res; }

        // Deny rule takes precedence on allow rules, no matter where (don't check furthur permissions).
        if (res < 0) { return -1; }
    }
    return res;
};

/* Internal: return permission status of a role. */
KWMOPermissions.prototype._roleCheck = function(role_name, perm_name)
{
    return this._ruleListCheck(this._roles[role_name], perm_name);
};

/* Internal: return permission status of a single rule. */
KWMOPermissions.prototype._ruleCheck = function(rule_type, rule_value, perm_name)
{
    if (perm_name.substring(0, rule_value.length) == rule_value)
    {
        if (rule_type == 'a') { return 1; }
        else if (rule_type == 'd') { return -1; }
    }
    return 0;
};

/* Chat widget namespace */
var ChatWidget = function()
{
    var _debug_id = 'ChatWidget';
    var block_id = 'chat_block';
    var _enabled = false;
    var chat_messages = [];
    var last_evt_chat_id = 0;

    return {

        // Channel ID.
        channel_id : 0,

        // Initialize widget.
        init : function()
        {
            // Disable widget by default.
            ChatWidget.disable(true);
        },

        // Show widget.
        show : function()
        {
            DebugConsole.debug(5, _debug_id, "Showing widget.");
            $(block_id).show();
        },

        // Hide widget.
        hide : function()
        {
            DebugConsole.debug(4, _debug_id, "Hiding widget.");
            $(block_id).hide();
        },

        // Update perms.
        update_perms : function(perms)
        {
            if (perms.hasPerm('chat.list.channel.' + ChatWidget.channel_id))
            {
                ChatWidget.enable();
            }
            else
            {
                ChatWidget.disable();
            }
        },

        // Enable widget.
        enable : function(force) 
        {
            if (force || !_enabled)
            {
                DebugConsole.debug(4, _debug_id, "Enabling widget.");

                // Add state current flags and parameters.
                // Note: _add_  actually behaves as _set_ so this code can be called several times.
                state_add_cur_flags(STATE_WANT_CHAT);
                state_add_cur_params({'chat_channel_id' : ChatWidget.channel_id});

                if (! $(block_id).visible()) { ChatWidget.show(); }

                _enabled = true; 
            }
            else
            {
                DebugConsole.debug(1, _debug_id, "Widget already enabled.");
            }
        },

        // Disable widget.
        disable : function(force) 
        { 
            if (force || _enabled)
            {
                DebugConsole.debug(4, _debug_id, "Disabling widget.");

                //if ($(block_id).visible()) { ChatWidget.hide(); }

                _enabled = false;
            }
            else
            {
                DebugConsole.debug(1, _debug_id, "Widget already disabled.");
            }
        },

        // Update chat messages list.
        // TODO: refactor
        update_messages : function(res)
        {
            DebugConsole.debug(7, _debug_id, "update_messages() called");
            //DebugConsole.debug(12, _debug_id, "update_messages() called with data: '" + dump(res) + "'.");

            var evt_id = res["last_evt"];
            ChatWidget.last_evt_chat_id = evt_id;
            var mode = res["data"]["mode"];
            var list = res['data']["messages"];
            var i;
            var msgTemplateHTML = get_by_id('messages_template_container').innerHTML;
            
            DebugConsole.debug(12, _debug_id, "update_messages() mode: '" + mode + "'.");

            if (mode == "all")
            {
                // Reset the internal list of chat messages.
                chat_messages = [];
            }
            for (i=0; i<list.length; i++)
            {
                // Append all messages to the internal list of chat messages.
                chat_messages[chat_messages.length] = list[i];
            }

            if (chat_messages.length > 0)
            {    
                //TODO: replace this with JST
                str = '';
                
                for (i=0; i<chat_messages.length; i++) //var x in chat_messages)
                {
                    cm = chat_messages[i];
                    var tmpStr = msgTemplateHTML;
                    
                    tmpStr = tmpStr.replace("var_message_text", cm["msg"]);
                    tmpStr = tmpStr.replace("var_message_user", get_user_or_email(cm["user_id"]));
                    tmpStr = tmpStr.replace("var_message_date_time", format_date_iso(cm["date"], false));
                    str += tmpStr
                }
                
                obj = get_by_id("chat_msg_list");
                if (obj)
                {
                    // // do not scroll down if user is reading something higher
                    //var scroll_down = (obj.scrollTop = obj.scrollHeight) ? 1 : 0
                    var scroll_down = 1;
                    
                    obj.innerHTML = str;
                    
                    // scroll down if wanted
                    if (scroll_down) { obj.scrollTop = obj.scrollHeight; }
                }
            }

            state_add_cur_params({"last_evt_chat_id":evt_id});
        }
    };
}();

/* Chat POST widget namespace */
var ChatPOSTWidget = function()
{
    var _debug_id = 'ChatPOSTWidget';
    var block_id = 'chat_post_block';
    var text_input_id = 'new_msg_input';
    var post_button_id = 'chat_send_link';
    var _enabled = false;

    return {

        // Initialize widget.
        init : function()
        {
            // Disable widget by default.
            ChatPOSTWidget.disable(true);
        },

        // Show widget.
        show : function()
        {
            DebugConsole.debug(5, _debug_id, "Showing widget.");
            $(block_id).show();
        },

        // Hide widget.
        hide : function()
        {
            DebugConsole.debug(4, _debug_id, "Hiding widget.");
            $(block_id).hide();
        },

        // Update perms.
        update_perms : function(perms)
        {
            if (perms.hasRole('freeze'))
            {
                ChatPOSTWidget.disable(true, 'Workspace is moderated.');
            }
            else if (perms.hasPerm('chat.post.channel.' + ChatWidget.channel_id))
            {
                ChatPOSTWidget.enable();
            }
            else
            {
                ChatPOSTWidget.disable();
            }
        },

        // Enable chat POST widget.
        enable : function(force)
        {
            if (force || !_enabled)
            {
                DebugConsole.debug(4, _debug_id, "Enabling widget.");

                $('new_msg_input').disabled = false;
                $('chat_send_nolink').hide();
                $('chat_send_link').show();

                // Show widget if needed.
                if (! $(block_id).visible()) { ChatPOSTWidget.show(); }
		$('new_msg_input').focus();
                _enabled = true;
            }
            else
            {
                DebugConsole.debug(1, _debug_id, "Widget already enabled.");
            }
        },

        // Disable chat POST widget.
        disable : function(force, tooltipmsg)
        {
            if (tooltipmsg == undefined) { tooltipmsg = 'Chat post is disabled.'; }
            if (force || _enabled)
            {
                DebugConsole.debug(4, _debug_id, "Disabling widget.");

                // Hide widget if needed.
                //if ($(block_id).visible()) { ChatPOSTWidget.hide(); }

                $('new_msg_input').value = '';
                $('new_msg_input').disabled = true;
                $('chat_send_link').hide();
                $('chat_send_nolink').title = tooltipmsg;
                $('chat_send_nolink').show();

                _enabled = false;
            }
            else
            {
                DebugConsole.debug(1, _debug_id, "Widget already disabled.");
            }
        },

        // Post a chat message.
        post_chat_message_request : function(msg)
        {
            DebugConsole.debug(6, _debug_id, "post_chat_message_request() called");

            if (msg && msg.length > 0)
            {
                DebugConsole.debug(3, _debug_id, "Posting chat message.");

                ChatPOSTWidget.disable();

                // Send the request.
                var url = get_url_path('teambox_post_chat') + "/" + kws_id;
                DebugConsole.debug(6, _debug_id, "Post chat url: '" + url + "'.");
                // TODO not finished - need to avoid double updates
                //new KAjax('chat_post').jsonRequest(url, {'channel_id' : ChatWidget.channel_id, 'last_evt_chat_id' : ChatWidget.last_evt_chat_id, 'message' : msg,
                //    'state_req_id' : ++state_req_id },
                //    ChatPOSTWidget.chat_post_success, handle_kajax_exception);
                new KAjax('chat_post').jsonRequest(url, {'channel_id' : ChatWidget.channel_id, 'message' : msg },
                    ChatPOSTWidget.chat_post_success, ChatPOSTWidget.chat_post_failure, 'POST');
            }
            return false;
        },

        // Chat post success callback.
        chat_post_success : function(res_dict, transport)
        {
            try
            {
                ChatPOSTWidget.enable();

                DebugConsole.debug(1, _debug_id, "Chat post succeeded.");

                // TODO Handle update.
                // (Disabled - not totally finished).
                //handle_updater_state(res_dict['updater']);
            }
            catch(err)
            {
                DebugConsole.error(_debug_id, "chat_post_success(): error: '" + err + "'.");
            }
        },
        
        // Chat post faliure callback.
        chat_post_failure: function(transport)
        {
            try
            {
                ChatPOSTWidget.enable();
                
                DebugConsole.debug(1, _debug_id, "Chat post failed.");
                
            }
            catch(err)
            {
                DebugConsole.error(_debug_id, "char_post_failure(): error: '" + err + "'.");
            }
            
        },

        // Chat post submit call.
        chat_send_clicked : function() 
        {
            if (_enabled)
            {
                var msg = $('new_msg_input').value;
                if (!msg.empty()) { ChatPOSTWidget.post_chat_message_request(msg); }
            }
            else
            {
                DebugConsole.debug(1, _debug_id,  "Ignoring chat post: widget is disabled.");
            }
        }

    };
}();


