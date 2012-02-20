
// untested - might try later
function form_auto_focus(form)
{
    // set the focus to first element of <form>
    if(form.elements[0] != null)
    {
        var i;
        var max = form.length;
        for(i=0; i<max; i++)
        {
            if( form.elements[i].type != "hidden" &&
                !form.elements[i].disabled &&
                !form.elements[i].readOnly )
            {
                form.elements[i].focus();
                break;
            }
        }
    }
}


