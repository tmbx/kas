/*
   Handles communications between an the VNC applet and the parent browser using javascript.
*/

package com.teambox.vnc;

import java.applet.Applet;

// from java_common
import com.teambox.util.JSComm;

public class JSCommVNC extends JSComm 
{
    // Javascript function names
    private String js_status_func;
    private String js_server_log_func;

    // VNC status codes
    public static final int VNC_STATUS_NONE = 1;
    public static final int VNC_STATUS_TESTING = 2;
    public static final int VNC_STATUS_APPLET_LOADED = 3;
    public static final int VNC_STATUS_RESOLVING = 4;
    public static final int VNC_STATUS_CONNECTING = 5;
    public static final int VNC_STATUS_PASSED = 6;
    public static final int VNC_STATUS_CONNECTED = 7;
    public static final int VNC_STATUS_FAILED = 8;
    public static final int VNC_STATUS_CLOSED = 9;

    // Constructor(s)
    public JSCommVNC(Applet o)
    {
        super(o);
    }

    // Set the javascript status function.
    public void set_js_status_func(String js_status_func)
    {
        this.js_status_func = js_status_func;
    }

    // Send statis code to javascript.
    public void status(int code)
    {
        super.func_call(this.js_status_func, code);
    }

    // Send status (code,message) to javascript.
    public void status(int code, String s)
    {
        super.func_call(this.js_status_func, code, s);
    }

    // Set the javascript server log function.
    public void set_js_server_log_func(String js_server_log_func)
    {
        this.js_server_log_func = js_server_log_func;
    }

    // Send server log to javascript.
    public void server_log(String s)
    {
        super.func_call(this.js_server_log_func, s);
    }
}

