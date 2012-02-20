// Unresettable VNC status variables
var vnc_test_passed = false;
var vnc_active_sessions = null;
var vnc_test_applet_html = null;
var vnc_test_started = false;
var vnc_test_timer = null;
var java_tested = false;
var last_vnc_session_id = null;
var vnc_ignore_applet = true;
//var vnc_viewer_timer = null;
var old_vnc_count = 0;

// VNC status constants (test and vnc viewer status codes are mixed)
var TIMEOUT_SECONDS = 25;
var VNC_STATUS_NONE = 1;
var VNC_STATUS_TESTING = 2;
var VNC_STATUS_APPLET_LOADED = 3;
var VNC_STATUS_RESOLVING = 4;
var VNC_STATUS_CONNECTING = 5;
var VNC_STATUS_PASSED = 6;
var VNC_STATUS_CONNECTED = 7;
var VNC_STATUS_FAILED = 8;
var VNC_STATUS_CLOSED = 9;
var VNC_STATUS_IGNORE = 10;
var VNC_STATUS_JAVA_TIMEOUT = 11

function set_vnc_idle() { set_class_by_id('vnc_help', 'idle'); set_class_by_id('vnc_status',''); set_vnc_status(''); clear_vnc_log(); }
function set_vnc_pending(s) { set_class_by_id('vnc_help', 'pending'); set_class_by_id('vnc_status','app-alert loading'); set_vnc_status(s); }
function set_vnc_passed(s) { set_class_by_id('vnc_help', 'passed'); set_class_by_id('vnc_status','app-alert connected'); set_vnc_status(s); }

//function set_vnc_failed(s) { set_class_by_id('vnc_help', 'failed'); set_class_by_id('vnc_status','app-alert error'); set_vnc_status(s); }
//No need to set vnc_status on failure after new design, just show errors
function set_vnc_failed(s) { set_class_by_id('vnc_help', 'failed'); set_class_by_id('vnc_status',''); set_vnc_status(''); }

function show_failed(s) { document.getElementById("vnc_failed_reason_text").innerHTML = s; }
function set_vnc_failed_java(s) { set_class_by_id('vnc_failed', 'java'); show_failed(s); }
function set_vnc_failed_other(s) { set_class_by_id('vnc_failed', 'other'); show_failed(s); }

function show_vnc_log() { DebugConsole.debug(1, null, "Showing vnc logs."); set_class_by_id('vnc_log', 'show'); }
function hide_vnc_log() { DebugConsole.debug(1, null, "Hiding vnc logs."); set_class_by_id('vnc_log', 'hide'); }

// Log messages from the DebugConsole.
function vnc_debug(dlevel, namespace, message)
{
    if (vnc_ignore_applet)
    {
        // Applet has been (virtually) closed. Ignore all messages from it.
        return;
    }

    vnc_log(message);
}

// Set VNC status in vnc status div.
function set_vnc_status(status)
{
    get_by_id("vnc_status").innerHTML = status;
}

// Reset vnc messages
function clear_vnc_log()
{
    document.getElementById("vnc_messages").innerHTML = '';
}

// Add vnc message (local)
function vnc_log(s)
{
    document.getElementById("vnc_messages").innerHTML += "<div>" + s + "</div>";
    DebugConsole.debug(1, null, "DEBUG MESSAGE " + s);
}

// Set status to ready.
function set_vnc_ready()
{
    set_vnc_passed("Ready");
}
 
// Set VNC status code.
function set_vnc_test_status(code, s)
{
    if (vnc_ignore_applet && code != VNC_STATUS_APPLET_LOADED && code != VNC_STATUS_JAVA_TIMEOUT) 
    {
        // Applet has been (virtually) closed. Ignore all messages from it.
        return;
    }

    if (code == VNC_STATUS_APPLET_LOADED)
    {
        // Clear timeout timer
        clearTimeout(vnc_test_timer)

        // Applet loaded.
        DebugConsole.debug(1, null, "VNC test applet loaded.");

        java_tested = true;

        // Dont ignore messages from applet (used to ignore old applets when we start new ones because we can't
        // kill them safely).
        vnc_ignore_applet = false;
    }
    else if (code == VNC_STATUS_PASSED)
    {
        // Test passed.
        DebugConsole.debug(1, null, "VNC test has succeeded.");

        vnc_test_passed = true;
        set_vnc_ready();

        refresh_vnc_list();

        stop_applet();
    }
    else if (code == VNC_STATUS_FAILED || code == VNC_STATUS_JAVA_TIMEOUT)
    {
        // Test failed.
        DebugConsole.error(null, "VNC test has failed.");

        set_vnc_failed("Not functional...");
        if (code == VNC_STATUS_FAILED)
        {
            if (s) { set_vnc_failed_other(s); }
            else { set_vnc_failed_other("Unexpected error."); }
        }
        else if (code == VNC_STATUS_JAVA_TIMEOUT)
        {
            set_vnc_failed_java("Java plugin could not load.");
        }
        refresh_vnc_list();

        stop_applet();
    }
    else
    {
        // Invalid status code
        DebugConsole.error(null, "Received invalid vnc test status: " + code + ".");
        set_vnc_test_status(VNC_STATUS_FAILED);
    }
}

// Set status of VNC viewer
function set_vnc_viewer_status(code, s)
{
    if (vnc_ignore_applet && code != VNC_STATUS_APPLET_LOADED && code != VNC_STATUS_JAVA_TIMEOUT)
    {
        // Applet has been (virtually) closed. Ignore all messages from it.
        vnc_session_connected = false;
        return;
    }

    if (code == VNC_STATUS_APPLET_LOADED)
    {
        // Clear timeout timer
        //clearTimeout(vnc_viewer_timer)

        // Applet loaded.
        DebugConsole.debug(1, null, "VNC viewer applet loaded.");

        // Dont ignore messages from applet (used to ignore old applets when we start new ones because we can't 
        // kill them safely).
        vnc_ignore_applet = false;
    }
    else if (code == VNC_STATUS_CONNECTING || code == VNC_STATUS_CONNECTED)
    {
        if (code == VNC_STATUS_CONNECTED)
        {
            set_vnc_passed("Connected!");
            vnc_session_connected = true;
            window.onbeforeunload = function() {
                if (vnc_session_connected) {
                    return 'You are connected to a screen sharing session which will be terminated if you do.';
                }
            }
        }
    }
    else if (code == VNC_STATUS_CLOSED)
    {
        set_vnc_ready();
        stop_applet();
        vnc_session_connected = false;
    }
    else if (code == VNC_STATUS_FAILED || code == VNC_STATUS_JAVA_TIMEOUT)
    {
        DebugConsole.error(null, "VNC viewer has failed.");

        set_vnc_failed("Failed...");
        if (code == VNC_STATUS_FAILED)
        {
            if (s) { set_vnc_failed_other(s); }
            else { set_vnc_failed_other("Unexpected error."); }
        }
        else if (code == VNC_STATUS_JAVA_TIMEOUT)
        {
            set_vnc_failed_java("Java plugin could not load.");
        }
        stop_applet();
        vnc_session_connected = false;
    }
    else
    {
        DebugConsole.error(null, "Received invalid vnc viewer status: " + code + ".");
        set_vnc_test_status(VNC_STATUS_FAILED);
        vnc_session_connected = false;
    }
}

// Send debugging informations to the server.
function vnc_server_log(s)    
{
    DebugConsole.debug(1, null, "Server log: '" + s + "'.");
    if (s.length > 0)
    {
        server_log(s);
    }
}

// This function handles a timeout when doing the VNC test.
function vnc_test_timeout()
{
    if (! java_tested)
    {
        DebugConsole.error(null, "VNC test applet load timeout.");
        set_vnc_test_status(VNC_STATUS_JAVA_TIMEOUT);
    }
}

// This function allows to restart the test (in case computer is slow).
function restart_vnc_test()
{
    set_vnc_idle();
    run_test_applet(vnc_test_applet_html);
} 

// This function runs the test applet.
function run_test_applet(code)
{
    DebugConsole.debug(1, null, "run_test_applet() called");

    // Change test status.
    set_vnc_pending("Checking compatibility with your system ...");

    // Show vnc help section.
    vnc_log("Testing screen sharing...");

    // Initiate a timeout callback.
    vnc_test_timer = setTimeout("vnc_test_timeout()", TIMEOUT_SECONDS * 1000);

    // Start test applet
    vnc_test_started = true;
    DebugConsole.debug(1, null, "Starting screen sharing test applet.");
    var obj = document.getElementById('vnc_app');
    obj.innerHTML = code; // load html code in a hidden div - will load the java vnc client
}

// This function stops applet.
function stop_applet()
{
    DebugConsole.debug(1, null, "Stopping the applet.");

    // Assumption: applets cannot be closed reliably (actually, this did't seem to work at all this way).
    //get_by_id("vnc_app").innerHTML = "";

    // Set a flag to ignore all messages/statuses from the applet.
    // Note: 
    // - Debug messages will still be available in the debug console... they just
    //   are no longer displayed in the vnc log (see vnc_debug()).
    // - When applet is restarted or is replaced by another applet, it seems the old applet is really killed before
    //   starting the new one, so it seems races are not possible in this case.
    vnc_ignore_applet = true;
}

// This function updates the active VNC sessions list.
function update_vnc_sessions(res)
{
    DebugConsole.debug(7, null, "update_vnc_sessions() called");
   
    var evt_id = res["last_evt"];
    var mode = res["mode"];
    var list = res["list"];
    var i, found;
  
    if (vnc_active_sessions != null)
    { 
        for(var j=0; j<vnc_active_sessions.length; j++)
        {
            // Detect which sessions were stopped.
            found = 0;
            for(var k=0; k<list.length; k++)
            {
                if (list[k].session_id == vnc_active_sessions[j].session_id) { found = 1; break; }
            }
            if (!found) 
            {
                if (vnc_active_sessions[j].session_id == last_vnc_session_id)
                {
                    var message = "Screen sharing session was ended by " + 
                        get_user_or_email(vnc_active_sessions[j].user_id) + ".";
                    GlobalMessage.warn(message=message);
                    last_vnc_session_id = null;
                }
            }   
        }
    }
 
    if (mode == "all")
    {
        // Reset the internal list of vnc sessions.
        vnc_active_sessions = [];
    }
    for (i=0; i<list.length; i++)
    {
        // Append all messages to the internal list of chat messages.
        vnc_active_sessions[vnc_active_sessions.length] = list[i];
    }
    
    if (vnc_active_sessions.length > 0 && !vnc_test_started && perms.hasPerm('vnc.connect'))
    {
        // Update global test applet code.
        vnc_test_applet_html = document.getElementById('vnc_test_applet_html').value;

        // Start test.
        vnc_test_applet_html = vnc_test_applet_html.replace(/applet_disabled/g, "applet");
        run_test_applet(vnc_test_applet_html);
    }
    else
    {
        // Refresh list
        refresh_vnc_list();
    }
    
    state_add_cur_params({"last_evt_vnc_id":evt_id});

    DebugConsole.debug(6, null, "update_vnc_sessions() finished");
}

// This function refreshes the VNC list.
function refresh_vnc_list()
{
    DebugConsole.debug(5, null, "refresh_vnc_list() called");

    var obj = get_by_id("vnc_list");
    var alert_for_vnc = false;

    //TODO: to be replace by JST

    var vnc_app_template = get_by_id("vnc_app_template_container").innerHTML;
    
    var read_only_app_template = get_by_id("read_only_app_template_container").innerHTML;
    
    str = '';
    if (vnc_active_sessions.length > 0)
    {

        var i = 0;
        for (i=0;i< vnc_active_sessions.length; i++)
        {
            var tempStr;
            
            vs = vnc_active_sessions[i];
            
            if (1) //vnc_test_passed == true)
            {
                tempStr = vnc_app_template;
            }
            else
            {
                // Test has failed, is not finished or has not loaded.
                // In all cases, vnc status is visible to know what's going on.
                tempStr = read_only_app_template;
            }
            
            
            tempStr = tempStr.replace("var_session_id", vs.session_id);
            tempStr = tempStr.replace("var_app_name", vs.subject);
            tempStr = tempStr.replace("var_vnc_user", get_user_or_email(vs.user_id));
            
            str += tempStr;
            
        }

        if (vnc_test_passed == true) { set_vnc_ready(); }

        DebugConsole.debug(6, null, "refresh_vnc_list(): Received " + i + " vnc sessions.");
        obj.innerHTML = str;
        if (vnc_test_passed == true && vnc_active_sessions.length > old_vnc_count)
        {
            var last_session = vnc_active_sessions[vnc_active_sessions.length-1];
            var tempStr = get_by_id("new_vnc_prompt_template_container").innerHTML;
            tempStr = tempStr.replace("var_session_id", last_session.session_id);
            tempStr = tempStr.replace("var_vnc_user", get_user_or_email(last_session.user_id));
            
            Modalbox.show(tempStr, {title:("Screen Sharing : " + last_session.subject)});
        }

    }
    else
    {
        set_vnc_idle();
        DebugConsole.debug(6, null, "refresh_vnc_list(): Received no vnc sessions.");
        obj.innerHTML = get_by_id("no_app_template_container").innerHTML;
    }
    
    if (Modalbox.initialized && vnc_active_sessions.length < old_vnc_count)
    {
        Modalbox.hide();
    }
    
    old_vnc_count = vnc_active_sessions.length;
}

// This function does a vnc request.
function vnc_request(session_id)
{
    // Remember last user ID so for the retry link.
    last_vnc_session_id = session_id;

    // Add one-time state flags and parameters.
    //state_add_one_time_flags(STATE_DO_VNC_REQUEST);
    //state_add_one_time_params({'vnc_user_id' : user_id});

    // Show vnc help section.
    set_vnc_idle();

    // Send the request.
    var url = get_url_path('vnc_session_start') + "/" + kws_id + "/" + session_id;
    DebugConsole.debug(6, null, "VNC connection request url: '" + url + "'.");
    new KAjax('vnc_conn').jsonRequest(url, {},
        vnc_request_success, handle_kajax_exception);
}

// This function restarts the vnc test or the vnc viewer applet.
function vnc_retry()
{
    if (vnc_test_passed) { vnc_request(last_vnc_session_id); }
    else { restart_vnc_test(); }
}

// This function handles a successful vnc request.
function vnc_request_success(res, transport)
{
    DebugConsole.debug(1, null, "VNC request successful... starting applet.");
    set_vnc_pending("Starting application...");
    vnc_log("Loading applet...");

    // Update global vnc applet code.
    var vnc_applet_html = document.getElementById('vnc_applet_html').value;
   
    var obj = document.getElementById('vnc_app');
    vnc_applet_html = vnc_applet_html.replace("var_teambox_auth", res["teambox_auth"]);
    vnc_applet_html = vnc_applet_html.replace(/applet_disabled/g, "applet");
    obj.innerHTML = vnc_applet_html // load html code in a hidden div - will load the java vnc client
}

