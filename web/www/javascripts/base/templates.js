/*
    Extracts templates from dom and reuse it at will.
*/

var dom_templates = {};
var templates = {};

// This function extracts (and detatches) templates from document.
// It allows changing id too, though it's not mandatory.
function dom_templates_extract(id, new_id)
{
    var obj, parent_obj, new_obj, use_id;

    // Set the id that will be used.
    use_id = id;
    if (new_id != undefined) { use_id = new_id; }

    // Extract the template and detatch it from document.
    obj = get_by_id(id);
    if (!obj) { alert("no such object id: '" + id + "'"); }
    parent_obj = obj.parentNode;
    new_obj = obj.cloneNode(true);
    parent_obj.removeChild(obj);

    // Set the new obj id
    new_obj.id = use_id;

    // Add to the templates list.
    dom_templates[new_obj.id] = new_obj;
}

// This function gets a dom template.
function dom_templates_get(id)
{
    return dom_templates[id].cloneNode(true);
}

// This function extracts (and detatches) templates from document.
// PARENT MUST HAVE ONLY ONE CHILD: THE TEMPLATE!!!!!
function templates_extract(id)
{
    var obj, parent_obj;

    // Extract the template and detatch it from document.
    obj = get_by_id(id);
    parent_obj = obj.parentNode;
    templates[id] = parent_obj.innerHTML;
    parent_obj.removeChild(obj);
}

// This function gets a template.
function templates_get(id)
{
    return templates[id];
}

