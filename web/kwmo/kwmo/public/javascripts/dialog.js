// Misc dialog boxes.

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

