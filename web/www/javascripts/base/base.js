// Get an object by id - should work with all browsers we want to support, and more
function get_by_id(objectId)
{
    if ($(objectId) ==null)
        return false;
    
    return $(objectId);
}

// Handles several onload events
// How to use: in your main page,
//  - add window.onload = run_onload_events;
//  - add something like add_onload_event("warn();"); for every onload events you want
function add_onload_event(func)
{
    on_load_events[on_load_events.length] = func;
}
function run_onload_events()
{
    for(var i=0; i<on_load_events.length; i++)
    {
        eval(on_load_events[i]);
    }
}
on_load_events = new Array();

// Set an event to a node.
function set_event_by_obj(obj, event_name, func)
{
    // Drop event if needed (Element.observe adds events instead of setting them only once).
    // Note: Element.stopObserving() works even if no event is set.
    Element.stopObserving(obj, event_name);

    // Add submit event.
    Element.observe(obj, event_name, func);
}

// Set an event to a node, by ID.
function set_event_by_id(id, event_name, func)
{
    set_event_by_obj(get_by_id(id), event_name, func);
}

// Set an object class.
function object_set_class(obj, classname)
{
    obj.className = classname;
}

// Set the class by ID.
function set_class_by_id(id, classname)
{
    var obj = get_by_id(id);
    object_set_class(obj, classname);
}

// Add a class to an object
function object_add_class(obj, classname)
{
    if (obj.className)
    {
        class_arr = obj.className.split(" ");
        class_arr.push(classname);
        classname = class_arr.join(" ");
    }
    obj.className = classname;
}

// Add a class by ID
function add_class_by_id(id, classname)
{
    var obj = get_by_id(id);
    object_add_class(obj, classname);
}

// Check if object having specified Id has class.
function has_class_by_id(id, clsName) 
{
    var obj = get_by_id(id);
    return has_class_by_obj(obj);
}

// Check if object has class.
function has_class_by_obj(obj, clsName)
{
    if (obj.className)
    {
        var class_arr = obj.className.split(" ");

        for (j=0; j<class_arr.length; j++)
        { 
            if (class_arr[j] == clsName)
            {
                return true;
            }
        }
    }
    return false;
}

// Removes a class from an object
function object_remove_class(obj, classname)
{
    if (obj.className)
    {
        class_arr = obj.className.split(" ");
        new_class_arr = new Array();

        for (j=0; j<class_arr.length; j++)
        {
            if (class_arr[j] != classname)
            {
                new_class_arr.push(class_arr[j]);
            }
        }

        obj.className = new_class_arr.join(" ");
    }
}

// Removes a class by ID
function remove_class_by_id(id, classname)
{
    var obj = get_by_id(id);
    object_remove_class(obj, classname);
}


// Give the focus to an object having id <id>
function focus_by_id(id)
{
    var obj = get_by_id(id);
    obj.focus();
}

// Show an element
function show_by_obj(obj)
{
    obj.style.display = '';
    object_remove_class(obj, "hidden");
}

// Show an element by id
function show_by_id(id)
{
    var obj = get_by_id(id);
    show_by_obj(obj);
}

// Hide an element
function hide_by_obj(obj)
{
    obj.style.display = 'none';
    object_add_class(obj, "hidden");
}

// Hide an element by id
function hide_by_id(id)
{
    var obj = get_by_id(id);
    hide_by_obj(obj);
}

// Check if object having the specified Id i s visible.
function is_visible_by_id(id)
{
    var obj = get_by_id(id)
    return is_visible_by_obj(obj);
}

// Check if object is visible.
function is_visible_by_obj(obj)
{
    return ! has_class_by_obj(obj, "hidden");
}

// Toogle show/hide by id.
function toggle_show_by_id(id)
{
    var obj = get_by_id(id);
    return toggle_show_by_obj(obj);
}

// Toogle show/hide by obj.
function toggle_show_by_obj(obj)
{
    if (has_class_by_obj(obj, "hidden"))
    {
        show_by_obj(obj);
        //return toggled value on
        return true;
    }
    else
    {
        hide_by_obj(obj);
        //return toggled value off
        return false;
    }
}

// Set value by id
function set_value_by_id(id, value)
{
    var obj = get_by_id(id);
    obj.value = value;
}

// Set content (innetHTML) by id
function set_content_by_id(id, content)
{
    var obj = get_by_id(id);
    obj.innerHTML = content;
}

// Get value by id
function get_value_by_id(id)
{
    var obj = get_by_id(id);
    if (obj) { return obj.value; }
    return null;
}

// Get content by id
function get_content_by_id(id)
{
    var obj = get_by_id(id);
    if (obj) { return obj.innerHTML; }
    return null;
}

// This function removes a document node by id.
function remove_dom_id(id)
{
    var obj = get_by_id(id);
    var parent_obj = obj.parentNode;
    parent_obj.removeChild(obj);
}

// This function disables an object (link?).
function disable_by_id(id) { var obj = get_by_id(id); object_disable(obj); }
function enable_by_id(id) { var obj = get_by_id(id); object_enable(obj); }
function object_disable(obj) { obj.disabled = true; }
function object_enable(obj) { obj.disabled = false; }

// Maps function(node, arguments) to all child text nodes of a node (default to document) which are not within a script tag.
function text_nodes_map(parent_node, func, args)
{
    if (!parent_node) { parent_node = document; }
    var childs = parent_node.childNodes;
    var i;

    for (i=0; i<childs.length; i++)
    {
        node = childs[i];
        if (node.nodeType == 3)
        {
            func(node, args);
        }
        else if (node.nodeType == 1)
        {
            if (node.tagName !== "SCRIPT" && node.tagName !== "STYLE")
            {
                text_nodes_map(node, func, args);
            }
        }
    }
}

// Replace all white spaces for non-breakable html entity '&nbsp;'.
function space_to_nbsp(s)
{
    return s.replace(' ', '&nbsp;');
}

// Replace all non-breakable html entities '&nbsp;' for white spaces.
function nbsp_to_space(s)
{
    return s.replace('&nbsp;', ' ');
}

// Merge two arrays.
function array_merge(a, b)
{
    var i;
    var z = [];
    for (i=0; i<a.length; i++) { z[z.length] = a[i]; }
    for (i=0; i<b.length; i++) { z[z.length] = b[i]; }
    return z;
}

// Replace text in a text node.
function text_nodes_replace(node, args)
{
    node.nodeValue = node.nodeValue.replace(args.search, args.replace);
}

// Find absolute object position (top, left).
function find_obj_position(obj)
{
    var curleft = curtop = 0;
    if (obj.offsetParent)
    {
        curleft = obj.offsetLeft
        curtop = obj.offsetTop
        while (obj = obj.offsetParent) 
        {
            curleft += obj.offsetLeft
            curtop += obj.offsetTop
        }
    }
    return [curleft,curtop];
}

// Timer class
var KTimer = function()
{        
    // private property: is timer enabled or not
    this._enabled = false; //new Boolean(false);
    
    // private property: timer tick counter
    this._counter = 0;

    // private member variable: hold interval id of the timer
    var _timer_id = 0;
    
    // private member variable: hold instance of this class
    var _this;

    // public property: how many milliseconds to wait between every tick
    this.interval = 1000;

    // public property: how many times to tick - set to 0 if you don't want the timer to stop
    this.times = 0;
   
    // public method: override to set a real tick function
    this.tick = function()
    {
        alert("TICK!");
    }
 
    // public method: start timer
    this.start = function()
    {

        _this = this;

        _this._enabled = true; //new Boolean(true);

        _this._timer_id = setInterval(function() { _this._tick(); }, _this.interval);
        //setTimeout(function() { _this._tick(); }, _this.interval);
    };
    
    // public method: stop timer
    this.stop = function()
    {            
        _this._enabled = false; //new Boolean(false);
        clearInterval(_this._timer_id);
    };

    // private method: internal tick
    this._tick = function()
    {
        _this._counter = _this._counter + 1;
        if (_this.times > 0 && _this._counter > _this.times)
        {
            _this.stop();
        }
        else
        {
            _this.tick();
            //setTimeout(function() { _this._tick(); }, _this.interval);
        }
    };
};

// List map.
function list_map(objects, map_func)
{
    var r = [];
    for (var i=0; i<objects.length; i++)
    {
        r[r.length] = map_func(objects[i]);
    }
    return r;
};

// Page reload.
function page_force_reload()
{
    if (navigator.vender && navigator.vendor.search('Apple') > -1)
        window.location.href = window.location.href;
    else
        window.location.reload(true);
}

// Page redirect.
function page_redirect(url)
{
    window.location.href = url;
}

// Hash map.
function hash_map(objects, map_func)
{
    var r = {};
    var a;
    for (var i=0; i<objects.length; i++)
    {
        a = map_func(objects[i]);
        r[a[0]] = a[1];
    }
    return r;
};

// UIMessage object.
var UIMessage = function()
{
    this.type = null;
    //this.title = null;
    this.message = null;
    this.hide_after_ms = null;
};
UIMessage.prototype.reset = function()
{
    this.type = null;
    //this.title = null;
    this.message = null;
    this.hide_after_ms = null;
};
UIMessage.prototype.from_dict = function(d)
{
    this.reset();

    if (d.type) { this.type = d.type; }
    //if (d.title) { this.title = d.title; }
    if (d.message) { this.message = d.message; }
    if (d.hide_after_ms) { this.hide_after_ms = d.hide_after_ms; }

    // Return this, although changes happen in place too.
    return this;
};
UIMessage.prototype.to_dict = function()
{
    //return {'type' : this.type, 'title' : this.title, 'message' : this.message, 'hide_after_ms' : this.hide_after_ms};
    return {'type' : this.type, 'message' : this.message, 'hide_after_ms' : this.hide_after_ms};
};


/*
* Function : dump()
* Arguments: The data - array,hash(associative array),object
*    The level - OPTIONAL
* Returns  : The textual representation of the array.
* This function was inspired by the print_r function of PHP.
* This will accept some data as the argument and return a
* text that will be a more readable version of the
* array/hash/object that is given.
*/
function dump(arr,level)
{
    var dumped_text = "";
    if (!level) { level = 0; }

    //The padding given at the beginning of the line.
    var level_padding = "";
    for(var j=0;j<level+1;j++) { level_padding += "    "; }

    if( typeof(arr) == 'object')
    {
        //Array/Hashes/Objects
        for(var item in arr)
        {
            var value = arr[item];

            if(typeof(value) == 'object')
            {
                //If it is an array,
                dumped_text += level_padding + "'" + item + "' ...\n";
                dumped_text += dump(value,level+1);
            }
            else
            {
                dumped_text += level_padding + "'" + item + "' => \"" + value + "\"\n";
            }
        }
    }
    else
    {
        //Strings/Chars/Numbers etc.
        dumped_text = "===>"+arr+"<===("+typeof(arr)+")";
    }
    return dumped_text;
}

function dump_level0_keys(arr)
{
    var t = '';
    for (key in arr)
    {
        t += key + ' ';
    }
    return t;
}
