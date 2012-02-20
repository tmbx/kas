/*
    Debug console namespace

    html signature:

        div.dc-menu: menu container
            input[button].dc-menu-clear: clear button
            input[button].dc-menu-levelup: debug level +
            input[button].dc-menu-leveldown: debug level -
            input[button].dc-menu-close: pause updates
            input[button].dc-menu-close: play updates
            input[button].dc-menu-close: close debug console

            span.dc-menu-autoscroll-label: "Auto-Scroll" label
            input[button].dc-menu-autoscroll-checkbox: auto-scroll checkbox

        div.dc-content: scrollable container
            div.dc-msgs-container: messages container
                div.dc-msg-row: line
                    span.dc-msg-date: debug message date
                    span.dc-msg-dlevel: debug level
                    span.dc-msg-namespace: namespace for debug message
                    span.dc-msg-message: debug message

    sample style:
            .dc-console { width: 500px; height: 300px; border: 1px solid black; }
            .dc-menu { background-color: #eeeeee; height: 20px;}
            .dc-menu a { padding-left: 0.3em; padding-right: 0.3em; }
            .dc-menu a.dc-menu-close { float: right; }
            .dc-content { height: 280px; overflow: auto; }
            .dc-msg-row span { padding-left: 0.3em; padding-right: 0.3em; }
            .dc-msg-namespace { display: inline-block; width: 5em; }
            .dc-msg-message { white-space: nowrap; }
*/

// Debug console namespace
var DebugConsole = function()
{
    // Configuration values
    var element_id = null;
    var records_history = 3000;
    var view_history = 1000;
    var view_queue_history = 1000;
    var level = 4;

    // Misc
    var view_count;
    var registered_exporters = new Array();
    var messages = [];
    var incoming_queue = [];
    var view_queue = [];
    var export_queue = [];
    var auto_scroll = true;
    var level = 4;
    var locked = false;
    var viewLocked = true;

    // DOM nodes
    var o_main = null; // extented with prototype
    var o_menu = null;
    var o_menu_autoscroll_label = null;
    var o_menu_autoscroll = null;
    var o_menu_clear = null;
    var o_menu_close = null;
    var o_menu_levelup = null;
    var o_menu_leveldown = null;
    var o_menu_pause = null;
    var o_menu_play = null;
    var o_content = null;
    var o_msgs_container = null;

    /* Private functions */

    // This function returns a left padded string.
    // Parameters:   s: string to pad | c: character to use when padding | nb: total length of padded string
    function lpad(s, c, nb)
    {
        s = "" + s;
        len = s.length;
        p = '';
        if (nb > len) { for (var i=0; i<(nb - len); i++) { p = p + c; } }
        return p + s;
    }

    // This function attaches an event to a node.
    function attatchEvent(node, evt, fnc)
    {
        if (node.addEventListener)
            node.addEventListener(evt, fnc, false);
        else if (node.attachEvent)
            node.attachEvent('on'+evt, fnc);
        else
            return false;
        return true;
    }

    // This function detaches an event from a node.
    function detachEvent(node, evt, fnc)
    {
        if (node.removeEventListener)
            node.removeEventListener(evt, fnc, false);
        else if (node.detachEvent)
            node.detachEvent('on'+evt, fnc);
        else
            return false;
        return true;
    }

    // This function converts a date object to a ISO string.
    var dateISO = function(dt)
    {
        var year = dt.getYear();
        if (year < 2000) { year += 1900; } // inconsistency with IE6
        var s = year + '-' + lpad(dt.getMonth()+1, 0, 2) + '-' + lpad(dt.getDate(), 0, 2)
            + '&nbsp;' + lpad(dt.getHours(), 0, 2) + ":" + lpad(dt.getMinutes(), 0, 2)
            + ":" + lpad(dt.getSeconds(), 0, 2);
        return s;
    }

    // Auto-scroll when a new message arrives... if auto-scrolling is enabled.
    var viewAutoScroll = function()
    {
        if (auto_scroll) { o_content.scrollTop = o_content.scrollHeight; }
    }

    // Init the messages container.
    var initMessagesContainer = function()
    {
        // Drop if existing.
        if (o_msgs_container) { o_content.removeChild(o_msgs_container); }

        // Create a new container.
        o_msgs_container = document.createElement("div");
        o_msgs_container.className = "dc-msgs-container";
        o_content.appendChild(o_msgs_container);
        view_count = 0;
    }

    // Append a message in the view.
    var viewAppendMessage = function(message)
    {
        if (message.dlevel <= level)
        {
            row = document.createElement("div");
            row.className = "dc-msg-row";

            tddate = document.createElement("span");
            tddate.className = "dc-msg-date";
            tddate.innerHTML = dateISO(message.date);
            row.appendChild(tddate);

            tddlevel = document.createElement("span");
            tddlevel.className = "dc-msg-dlevel";
            tddlevel.innerHTML = message.dlevel;
            row.appendChild(tddlevel);

            tdnamespace = document.createElement("span");
            tdnamespace.className = "dc-msg-namespace";
            tdnamespace.innerHTML = lpad(message.namespace, ' ', 10);
            row.appendChild(tdnamespace);

            tdmessage = document.createElement("span");
            tdmessage.className = "dc-msg-message";
            tdmessage.innerHTML = message.message;
            row.appendChild(tdmessage);

            o_msgs_container.appendChild(row);
            view_count += 1;
        }
    }

    // This function keeps the view queue small... just in case.
    var viewQueueHistoryClean = function()
    {
        if (view_queue.size > view_queue_history)
        { 
            for (var i=0; i<(view_queue.size-view_queue_history); i++)
            { 
                view_queue.shift(); 
            }
        } 
    }

    // This function purges old messages in the view.
    var viewHistoryClean = function()
    {
        if (view_count > view_history)
        {
            for (i=0; i<(view_count-view_history); i++)
            {
                var node = o_msgs_container.firstChild;
                o_msgs_container.removeChild(node);
                view_count--;
            }
        }

    }

    // Export message to all registered exporters.
    var exportMessage = function(message)
    {
        for (var i=0; i<registered_exporters.length; i++)
        {
            exporter = registered_exporters[i];
            if (
                (exporter.dlevel == null || message.dlevel <= exporter.dlevel)
                && (exporter.namespaces.length == 0 || exporter.in_namespaces(message.namespace))
               )
            {
                try { exporter.func(message.dlevel, message.namespace, message.message); }
                catch(e) { DebugConsole.error(message.namespace, "Could not export message to '" + exporter.func + "': '" + e + "'.", false); }
            }
        }
    }
 
    // Public DebugConsole namespace
    return {

    // Main updater timeout
    updater_timeout : null,

    // Debug message class
    DebugMessage : function(message, dlevel, namespace, date, do_export)
    {
        // defaults debug message properties
        if (!dlevel) { dlevel = 1; }
        if (!namespace) { namespace = "default"; }
        if (!date) { date = new Date(); }
        if (!do_export) { do_export = false; }

        // properties
        this.date = date;
        this.dlevel = dlevel;
        this.namespace = namespace;
        this.message = message;
        this.do_export = do_export;
    },

    // Debug external exporter class
    // This class represents an external exporter. When registered in the debug console,
    // new messages that match the update createrias will be sent to the exporter function.
    DebugExporter : function(f, dlevel, namespaces)
    {
        // default properties
        if (!dlevel) { dlevel = null; }
        if (!namespaces) { namespaces = new Array(); }

        // properties
        this.func = f;
        this.dlevel = dlevel;
        this.namespaces = namespaces;

        // This function checks if the provided namespace is in the instance namespaces property.
        this.in_namespaces = function(namespace)
        {
            for (var i=0; i<this.namespaces.length; i++)
            {
                if (namespace == this.namespaces[i]) { return true; }
            }
            return false;
        }
    },


    // This function shows the console.
    show : function() 
    {
        if (!DebugConsole.isVisible()) { show_by_obj(o_main); this.play(); }
    },

    // This function hides the console.
    hide : function() { hide_by_obj(o_main); },

    // This function toggles the console visibility.
    toggle : function() 
    {
        if (DebugConsole.isVisible()) { DebugConsole.hide(); }
        else { DebugConsole.show(); }
    },

    // This function clears the console messages.
    clear : function() 
    {
        locked = true;

        messages = [];
        incoming_queue = [];
        view_queue = [];
        export_queue = [];
        DebugConsole.play(); 

        locked = false;
    },

    // This function is an alias for the hide() function. It does NOT clears the messages.
    close : function() { DebugConsole.hide(); },

    // This function toggles the auto-scroll feature.
    toggleAutoScroll : function()
    {
        if (o_menu_autoscroll.checked)
        {
            auto_scroll = true;
        }
        else
        {
            auto_scroll = false;
        }
    },

    // This function disables the auto-scroll feature.
    disableAutoScroll : function()
    {
        auto_scroll = false;
        o_menu_autoscroll.checked = false;
    },

    // This function enables the auto-scroll feature.
    enableAutoScroll : function()
    { 
        auto_scroll = true;
        o_menu_autoscroll.checked = true;
    },

    // This function increases the debug level of the console. Messages with higher debug level
    // are still kept in memory but not showed.
    levelUp : function()
    {
        level += 1;
        DebugConsole.viewRefresh();
    },

    // This function reduces the debug level of the console. Messages with higher debug level
    // are still kept in memory but not showed.
    levelDown : function()
    {
        if (level > 0) { level--; }
        DebugConsole.viewRefresh();
    },

    // This function sets the debug level of the console.
    setDebugLevel : function(dl)
    {
        level = dl;
        DebugConsole.viewRefresh();
    },

    // This function pauses the console so messages can be read.
    pause : function()
    {
        o_menu_pause.disabled = true;
        o_menu_play.disabled = false;
        viewLocked = true;
    },

    // This function restarts the console after a pause.
    play : function()
    {
        o_menu_pause.disabled = false;
        o_menu_play.disabled = true;
        DebugConsole.viewRefresh();
        viewLocked = false;
    },

    // This function returns weither the debug console is visible or not.
    isVisible : function()
    {
        if (o_main) { return is_visible_by_obj(o_main); }
        return false;
    }, 

    // This function inits the debug console view.
    //   -  <e_id> must the the id of an existing element for instantiating the console view.
    viewInit : function(e_id)
    {
        viewLocked = true;

        // Default element id is "debug_console".
        if (e_id == null) { e_id = "debug_console"; }

        // Get the element object using jquery.
        var o = get_by_id(e_id);
        if (o) { o_main = o; }
        else { throw("Invalid debug console element."); }
        Element.extend(e_id);
        element_id = e_id;

        // Hide console (workaround for javascript not returning the right style.display value at load).
        this.hide();

        // Customize element
        object_add_class(o_main, 'dc-console');

        // Menu
        if (o_menu) { o_main.removeChild(o_menu); }

        o_menu = document.createElement("div");
        o_menu.className = "dc-menu";
        o_main.appendChild(o_menu);

        // Clear menu
        o_menu_clear = document.createElement("input");
        o_menu_clear.type = "button";
        o_menu_clear.value = "Clear";
        attatchEvent(o_menu_clear, "click", function() { DebugConsole.clear(); });
        o_menu_clear.className = "dc-menu-clear";
        o_menu.appendChild(o_menu_clear);

        // Debug level up menu
        o_menu_levelup = document.createElement("input");
        o_menu_levelup.type = "button";
        o_menu_levelup.value = "+";
        attatchEvent(o_menu_levelup, "click", function() { DebugConsole.levelUp(); });
        o_menu_levelup.className = "dc-menu-levelup";
        o_menu.appendChild(o_menu_levelup);

        // Debug level down menu
        o_menu_leveldown = document.createElement("input");
        o_menu_leveldown.type = "button";
        o_menu_leveldown.value = "-";
        attatchEvent(o_menu_leveldown, "click", function() { DebugConsole.levelDown(); });
        o_menu_leveldown.className = "dc-menu-leveldown";
        o_menu.appendChild(o_menu_leveldown);

        // Pause menu
        o_menu_pause = document.createElement("input");
        o_menu_pause.type = "button";
        o_menu_pause.value = "Pause";
        attatchEvent(o_menu_pause, "click", function() { DebugConsole.pause(); });
        o_menu_pause.className = "dc-menu-pause";
        o_menu.appendChild(o_menu_pause);

        // Play menu
        o_menu_play = document.createElement("input");
        o_menu_play.type = "button";
        o_menu_play.disabled = true;
        o_menu_play.value = "Play";
        attatchEvent(o_menu_play, "click", function() { DebugConsole.play(); });
        o_menu_play.className = "dc-menu-play";
        o_menu.appendChild(o_menu_play);

        // Close menu
        o_menu_close = document.createElement("input");
        o_menu_close.type = "button";
        o_menu_close.value = "Close";
        attatchEvent(o_menu_close, "click", function() { DebugConsole.close(); });
        o_menu_close.className = "dc-menu-close";
        o_menu.appendChild(o_menu_close);

        // Auto-scroll link
        o_menu_autoscroll_label = document.createElement("span");
        o_menu_autoscroll_label.className = "dc-menu-autoscroll-label";
        o_menu_autoscroll_label.innerHTML = "Auto-scroll";
        o_menu.appendChild(o_menu_autoscroll_label);
        o_menu_autoscroll = document.createElement("input");
        o_menu_autoscroll.className = "dc-menu-autoscroll-checkbox";
        o_menu_autoscroll.setAttribute("type", "checkbox");
        attatchEvent(o_menu_autoscroll, "click", function() { DebugConsole.toggleAutoScroll(); });
        o_menu.appendChild(o_menu_autoscroll);
        DebugConsole.enableAutoScroll();

        // Content
        o_content = document.createElement("div");
        o_content.className = "dc-content";
        Element.extend(e_id);
        o_main.appendChild(o_content);

        initMessagesContainer();

        viewLocked = false;
    },

    // This function rebuilds the messages list in the view.
    viewRefresh : function()
    {
        viewLockedCache = viewLocked
        viewLocked = true;

        // Empty the view queue because those messages are also in the messages list.
        view_queue = new Array();

        // Reset message container.
        initMessagesContainer();

        // Append messages.
        for (var i=0; i<messages.length; i++) 
        {
           if (messages[i].dlevel <= level) { viewAppendMessage(messages[i]); }
        }

        // Clean old messages exceeding history parameter.
        viewHistoryClean();

        // Scroll
        viewAutoScroll();

        viewLocked = viewLockedCache;
    },

    // This function adds a debug message to the incoming queue.
    debug : function(dlevel, namespace, message, do_export)
    {
        var exporter;

        if (do_export == null) { do_export = true; }

        // Append the message to the incoming queue.
        var dm = new DebugConsole.DebugMessage(message, dlevel, namespace, null, do_export);
        incoming_queue.push(dm)
    },

    // This function adds an error message to the incoming queue.
    error : function(namespace, message, do_export)
    {
        DebugConsole.debug(0, namespace, "Error: " + message, do_export);
    },

    // Register an exporter function.
    register_exporter : function(exporter)
    {
        registered_exporters.push(exporter);
    },
    
    // Unregister an exporter function.
    unregister_exporter : function(exporter)
    {
        narr = new Array();
        // Create a new array without matching exporter. Match againts the function name only.
        for (var i=0; i<registered_exporters.length; i++)
        {
            if (registered_exporters[i].func != exporter.func) { narr.push(registered_exporters[i]); }
        }
        registered_exporters = narr;
    },

    // This function performs update routines:
    // - clean messages based on the messages history parameter
    // - dispatch incoming messages
    //   - update view
    //   - export messages
    update : function()
    {
        // FIXME: not perfect 
        if (!locked)
        {
            var nb;

            // Clean message history.
            var over = messages.length - records_history;
            if (over > 0) { for (var i=0; i<over; i++) { messages.shift(); } }

            // Dispatch incoming messages queue.
            nb = incoming_queue.length;
            for (var i=0; i<nb; i++) 
            {
                message = incoming_queue.shift();

                // Append to the messages list.
                messages.push(message)

                // Append to the view queue.
                view_queue.push(message)

                // Append to the export queue.
                if (message.do_export) { export_queue.push(message); } 
            }

            // Clean view queue if its getting too big
            viewQueueHistoryClean();

            // Dispatch view messages queue.
            if (!viewLocked && DebugConsole.isVisible())
            {
                nb = view_queue.length;
                var scroll = false;
                for (var i=0; i<nb; i++) 
                {
                    message = view_queue.shift();
                    if (message.dlevel <= level) { scroll = true; viewAppendMessage(message); }
                }

                // Clean old messages exceeding history parameter.
                viewHistoryClean();
        
                // Auto-scroll
                if (scroll) { viewAutoScroll(); }
            }
     
            // Dispatch export messages queue.
            nb = export_queue.length;
            for (var i=0; i<nb; i++)
            {
                message = export_queue.shift();
                exportMessage(message);
            }
        }

        // Schedule next update.
        DebugConsole.updater_timeout = setTimeout('DebugConsole.update();', 50);
    }

    };

}(); // end of DebugConsole namespace

// Schedule first update.
DebugConsole.update();

