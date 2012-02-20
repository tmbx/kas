// KAjax object
var KAjax = function(name) 
{
    if (name == undefined) { this.debug_id = 'kajax'; }
    else { this.debug_id = 'kajax-' + name; } 
};

// Do a json ajax request.
//  url: regular url
//  parameters: {}
//  success_callback: should except (resulting dictionary, prototype ajax transport object)
//  failure_callback: should except (prototype ajax transport object)
KAjax.prototype.jsonRequest = function(url, parameters, success_callback, failure_callback, method)
{
    var THIS = this;
    try
    {
        if (parameters == null) { parameters = {}; }
        if (method == null) { method = 'GET';}
        if (typeof success_callback != 'function') 
        { 
            throw ("KAjax.jsonRequest(): success callback is mandatory and must be a function."); 
        }
        if (failure_callback == undefined) { failure_callback = null; }
        if (typeof failure_callback != 'function' && failure_callback != null) 
        { 
            throw ("KAjax.jsonRequest(): failure callback must be a function or null."); 
        }
        new Ajax.Request(url, {
            method : method,
            parameters : parameters,
            onSuccess : function(transport) { THIS._jsonSuccessCallback(transport, success_callback); },
            onFailure : function(transport) { THIS._jsonFailureCallback(transport, failure_callback); },
            requestHeaders : new Array('X-KAjax', '1')
        }); 
    }
    catch(e)
    {
        this.error("KAjax jsonRequest() error: '" + e + "'.");
    }
};

// Log an error using DebugConsole.
KAjax.prototype.error = function(msg) { DebugConsole.error(this.debug_id, msg); };

// Log debug information using DebugConsole.
KAjax.prototype.debug = function(level, msg) { DebugConsole.debug(level, this.debug_id, msg); };

// Internal JSON request success callback.
KAjax.prototype._jsonSuccessCallback = function(transport, callback)
{
    var func_name = "_jsonSuccessCallback";
    try
    {
        var res_dict = transport.responseText.evalJSON(sanitize = true);
        if (res_dict['jsonify_action'])
        {
            if (res_dict['jsonify_action'] == 'reload')
            {
                if (res_dict['jsonify_message'] != null)
                {
                    show_non_modal_dialog(
                        res_dict['jsonify_message'],
                        function() { page_force_reload(); }
                    );
                }
                else
                {
                    window.onbeforeunload = function() {};
                    page_force_reload();
                }
            }
            else if (res_dict['jsonify_action'] == 'redirect')
            {
                if (res_dict['jsonify_message'] != null)
                {
                    show_non_modal_dialog(
                        res_dict['jsonify_message'],
                        function() { window.onbeforeunload = function() {}; page_redirect(res_dict['jsonify_url']); }
                    );
                }
                else
                {
                    window.onbeforeunload = function() {};
                    page_redirect(res_dict['jsonify_url']);
                }
            }
        }
        else
        {
            if (res_dict['glob_msgs'])
            {
                GlobalMessage.show_message_dict_list(res_dict['glob_msgs']); 
            }
 
            callback(res_dict, transport);
        }
    }
    catch(err)
    {
        this.error(func_name + "() error: '" + err + "'.");
    }
};

// Internal JSON request failure callback.
KAjax.prototype._jsonFailureCallback = function(transport, callback)
{
    var func_name = "_jsonFailureCallback";
    try
    {
        // An error occured.
        this.error(func_name + "(): error, html status='" + transport.status + "'.");

        if (transport.getHeader('X-JSON-Exception'))
        {
            // Dump JSON exception and trace (if available) to the debug console.
            var res_dict = transport.responseText.evalJSON(sanitize = true);
            this.error(func_name + "(): server exception: type='" +
                res_dict['type'] + "', exception='" + res_dict['exception'] + "'.");
            for (var i=0; i<res_dict['trace'].length; i++) 
            {
                this.debug(4, func_name + "(): trace: '" + res_dict['trace'][i] + "'.");
            }
        }

        if (callback != null) { callback(transport); }
    }
    catch(err)
    {
        this.error(func_name + "() error: '" + err + "'.");
    }
};

