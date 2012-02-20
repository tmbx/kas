// Misc dialog boxes.

// Global message namespace - show global messages.
// This relies on UIMessage object from base.js.
// TODO:
// - history, history view
var GlobalMessage = function()
{
    return {
        show_message_object : function(uim)
        {
            // Create a div element containing the message.
            var e = document.createElement('div');
            var class_str = 'glob_message';
            if (uim.type) { class_str += ' ' + uim.type; }
            e.className = class_str;
            e.innerHTML = uim.message;

            // Append div to glob_messages div.
            $('glob_messages').appendChild(e);

            if (uim.hide_after_ms)
            {
                setTimeout(function() { $('glob_messages').removeChild(e); }, uim.hide_after_ms); 
            }
        },

        show_message_dict_list : function(l)
        {
            var uim;
            for (var i=0; i<l.length; i++)
            {
                uim = new UIMessage();
                uim.from_dict(l[i]);
                GlobalMessage.show_message_object(uim);
            }
        },

        info : function(message, hide_after_ms)
        {
            if (!hide_after_ms || hide_after_ms == undefined) { hide_after_ms = 5000; }
            var uim = new UIMessage();
            uim.type = 'info';
            uim.message = message;
            uim.hide_after_ms = hide_after_ms;
            GlobalMessage.show_message_object(uim);
        },

        warn : function(message, hide_after_ms)
        {
            if (!hide_after_ms || hide_after_ms == undefined) { hide_after_ms = 5000; }
            var uim = new UIMessage();
            uim.type = 'warn';
            uim.message = message;
            uim.hide_after_ms = hide_after_ms;
            GlobalMessage.show_message_object(uim);
        },

        error : function(message, hide_after_ms)
        {
            if (!hide_after_ms || hide_after_ms == undefined) { hide_after_ms = 5000; }
            var uim = new UIMessage();
            uim.type = 'error';
            uim.message = message;
            uim.hide_after_ms = hide_after_ms;
            GlobalMessage.show_message_object(uim);
        }
    };
}();

// Show global messages sent in HTML code.
function show_glob_messages_from_html_input()
{
    var glob_messages = $('glob_messages_input').value.evalJSON(sanitize = true);
    GlobalMessage.show_message_dict_list(glob_messages);
}

// Dialog box for showing status.
function dialog_status(title, message)
{
    DebugConsole.debug(1, null, "Status dialog: '" + message + "'.");

    if (!title) { title = "Status"; }
    if (!message) { message = "No message?"; }
    
    Modalbox.show('<span>'+message+'</span>', {title:title, width:400, afterLoad: function(){}});
    Modalbox.activate();
}

// Dialog box for showing an error.
function dialog_error(title, message)
{
    DebugConsole.error(null, "Error dialog: '" + message + "'.");

    if (!title) { title = "Error"; }
    if (!message) { message = "No message?"; }
    
    Modalbox.show('<span>'+message+'</span>', {title:title, width:400, afterLoad: function(){}});
    Modalbox.activate();
}

