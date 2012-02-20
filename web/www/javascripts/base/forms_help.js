var formHelpLastIdent = "";

// Find object position (left, top)
function formHelpFindPos(obj)
{
    var curleft = curtop = 0;
    if (obj.offsetParent) {
        curleft = obj.offsetLeft
        curtop = obj.offsetTop
        while (obj = obj.offsetParent) {
            curleft += obj.offsetLeft
            curtop += obj.offsetTop
        }
    }
    return [curleft,curtop];
}

// Show help window with content from another object.
function formHelpPopHelp(linkobj,helpident)
{
    // find the help window
    var helpbox = get_by_id("helpbox");

    if (helpbox.style.visibility == "visible" && formHelpLastIdent == helpident)
    {
        // window with the same content is already visible.. hide it
        helpbox.style.visibility = "hidden";
        return
    }

    formHelpLastIdent = helpident;

    // hide the help window before moving it if it's already open
    helpbox.style.visibility = "hidden";

    // get message from object with id <helpident>
    get_by_id("helpbox_message").innerHTML = get_by_id(helpident).innerHTML;

    // position help window
    helpbox = get_by_id("helpbox");
    var tmpxy, x, y; // tmpxy was used after having problems with explorer
    tmpxy = formHelpFindPos(linkobj);
    x = tmpxy[0];
    y = tmpxy[1];
    x = x - helpbox.offsetWidth;
    x = x - 15;
    y = y + 30;
    helpbox.style.left = x+"px";
    helpbox.style.top = y+"px";

    // display help window
    helpbox.style.visibility = "visible";
}

// Hide help window
function formHelpHideHelp()
{
    var helpbox = get_by_id("helpbox");
    helpbox.style.visibility = "hidden";
}


