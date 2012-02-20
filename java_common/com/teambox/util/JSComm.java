/*
    This class allows applets to communicate with their parent document through javascript.
    It implements the KDebug interface: it sends debug intormation to the java console and to javascript, if 
    a javascript debug function is set. This function must accept: ( (int) <debug level>, (string) namespace, (string) <message> ).

    Notes:
        - It is based on the javascript eval call, which sucks but suck less than the other methods I tried.
        - Sorry, I did not take notes about what was the problem when I chose to use it.
*/

package com.teambox.util;

import java.lang.System;
import java.applet.Applet;
import java.util.*;
import java.net.*;

// from lib/plugin.jar
import netscape.javascript.JSObject;

// local
import com.teambox.util.KDebug;
import com.teambox.util.KMisc;

public class JSComm implements KDebug
{
    // Debug level
    private int dlevel;

    // Applet reference for js communications
    private Applet applet_obj;

    // Javascript function names
    private String js_debug_func;
    private String js_log_func;
    private String js_error_func;

    // Constructors
    public JSComm(Applet o)
    {
        this.applet_obj = o;
        this.set_debug_level(1);
        this.js_debug_func = null;
        this.js_log_func = null;
        this.js_error_func = null;
    }

    // Prepare arguments for a javascript call.
    public static Collection prepare_call_args(Collection args) throws Exception
    {
        Object o;
        List<Object> new_args = new ArrayList<Object>();
        Iterator iter = args.iterator();

        // Transform elements of args so they can be joined to create a string representation
        // of a function call.
        while (iter.hasNext())
        {
            o = iter.next();
            if (o == null) { o = "null"; }
            else if (o instanceof String) { String s = (String) o; o = "'" + s.replace("'", "\\'").replace("\n", "+").replace("\r", "") + "'"; }   
            //else if (o instanceof String) { String s = (String) o; o = "'" + URLEncoder.encode((String) o, "ISO-8859-1") + "'"; }
            else if (o instanceof Number) { }
            else { throw new Exception("Invalid argument: '" + o + "'."); }
            new_args.add(o);
        }

        return new_args;
    }

    // Call a javascript function. Valid arguments can null, string and numbers.
    synchronized private void _func_call(String func, Collection args) throws Exception
    {
        ArrayList new_args = (ArrayList) prepare_call_args(args);
        String js_call = func + "(" + KMisc.list_join(new_args, ", ") + ")";
        JSObject.getWindow(this.applet_obj).eval("javascript:" + js_call);
        KMisc.sleep_ms(15); // It seems like (some browsers at least)
                            // do not support rapid successive calls.
    }

    // Call a javascript function. Valid arguments can null, string and numbers.
    public void func_call(String func, Object ... args)
    {
        try { _func_call(func, Arrays.asList(args)); }
        catch (Exception e) 
        { 
            if (func != this.js_debug_func)
            {
                // Send debug information about the missed call.
                this.debug(1, "jscomm", "func_call() exception: '" + e.getClass().toString() + 
                    ": '" + e.getMessage() + "'");
            }
            else
            {
                // Avoid loop...
                System.out.println("func_call() debug exception: '" + e.getClass().toString() + 
                    ": '" + e.getMessage() + "'");    
            }
            e.printStackTrace();
        }
    }

    // Add a cookie.
    /*
    private void set_cookie(String name, String value)
    {
        JSObject.getWindow(this.applet_obj).eval("document.cookie ='" +(String)arg +"';");
    }
    */

    // Set the javascript debug function
    public void set_js_debug_func(String s)
    {
        this.js_debug_func = s;
    }

    // Sets the debugging level
    public void set_debug_level(int dlevel)
    {
        this.dlevel = dlevel;
    }

    // Send debug message if debug level is high enough
    public void debug(int dlevel, String namespace, String s)
    {
        if (dlevel <= this.dlevel)
        {
            // Send debug message to the java console
            System.out.println(dlevel + ":" + namespace + ":" + s);

            if (this.dlevel > 0 && this.js_debug_func != null)
            {
                // Send debug message to javascript
                this.func_call(this.js_debug_func, dlevel, namespace, s);
            }
        }
    }

    // Set the javascript log function
    public void set_js_log_func(String s)
    {
        this.js_log_func = s;
    }

    // Send log message
    public void log(String s)
    {
        // Send log message to the java console
        System.out.println(s);

        if (this.js_log_func != null)
        {
            // Send log message to javascript
            this.func_call(this.js_log_func, s);
        }
    }

    // Set the javascript error function
    public void set_js_error_func(String s)
    {
        this.js_error_func = s;
    }

    // Send error message
    public void error(String s)
    {
        // Send error message to the java console
        System.err.println(s);

        if (this.js_error_func != null)
        {
            // Send error message to javascript
            this.func_call(this.js_error_func, s);
        }
    }
}

