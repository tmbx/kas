/*
    Temporary hidden iframe "class"
*/

var TempHiddenIFrame =  function()
{
    // State
    this.active = false;

    // Init parameters
    this.form = null;
    this.id = null;

    // Internal
    this._container = null;
    this._iframe = null;
    this._iframe_name = null;

    DebugConsole.debug(6, 'TempHiddenIFrame', "Instantiated.");
};

// Initialize object. The ID should be unique, if possible, to avoid problems.
// If no ID is provided one will be generated.
// Note: on_load is NOT reliable.
TempHiddenIFrame.prototype.prepare = function(form, id, on_submit, on_load)
{
    DebugConsole.debug(7, 'TempHiddenIFrame', "Initializing TempHiddenIFrame.");

    if (this.active) { throw "This TempHiddenIFrame object is active."; }

    this.form = form;
    this.id = id;
    if (!this.id) { this.id = Math.floor(Math.random() * 99999); }
    this.on_submit = on_submit;
    this.on_load = on_load;

    // Get rid of old iframe, if needed.
    this._removeIFrame(0);

    // Create iframe.
    this._createIFrame();

    DebugConsole.debug(6, 'TempHiddenIFrame', "Initialized TempHiddenIFrame.");
};

// Set this method as the onsubmit event of the form.
TempHiddenIFrame.prototype.submit = function()
{
    DebugConsole.debug(7, 'TempHiddenIFrame', "submit() called.");

    // Prevent double-clicks.
    if (this.active) { DebugConsole.debug(1, 'TempHiddenIFrame', "submit ignored: already active."); return false; }

    // Set object as active.
    this.active = true;

    // Return the on_submit callback, if any.
    if (this.on_submit) { this.on_submit(); }

    DebugConsole.debug(6, 'TempHiddenIFrame', "submit() finished.");

    return true;
};

// Terminate usage. This MUST be idempotent.
TempHiddenIFrame.prototype.terminate = function()
{
    DebugConsole.debug(7, 'TempHiddenIFrame', "terminate() called.");

    var THIS = this;

    // Delete iframe, if needed.
    if (this._iframe) 
    {
        Element.stopObserving(this._iframe, 'load');
        Event.observe(this._iframe, 'load', function() { setTimeout(function() { THIS._terminate(); }, 1 ); });
        this._iframe.src = '/html/blank.html'; 
    }

    DebugConsole.debug(6, 'TempHiddenIFrame', "terminate() finished.");
};

// Internal: terminate usage. This MUST be idempotent.
TempHiddenIFrame.prototype._terminate = function()
{
    DebugConsole.debug(7, 'TempHiddenIFrame', "_terminate() called.");

    // Remove iframe.
    //this._removeIFrame(1);

    // Set object as inactive.
    this.active = false;

    DebugConsole.debug(6, 'TempHiddenIFrame', "_terminate() finished.");
};

// Internal: create an hidden iframe in the current document, and set it as the target of the form.
TempHiddenIFrame.prototype._createIFrame = function()
{
    DebugConsole.debug(7, 'TempHiddenIFrame', "_createIFrame() called.");

    var THIS = this;

    // Create container.
    this._container = document.createElement('DIV');
    document.body.appendChild(this._container);

    // Create hidden iframe with __unique__ ID.
    this._iframe_name = 'thf' + this.id;
    this._container.innerHTML = '<iframe style="display:none"'
                                + ' src="about:blank"'
                                + ' id="' + this._iframe_name + '"'
                                + ' name="' + this._iframe_name + '"></iframe>';

    // Get iframe node.
    this._iframe = document.getElementById(this._iframe_name);

    // Set the iframe onload callback to an internal method.
    Element.stopObserving(this._iframe, 'load');
    Event.observe(this._iframe, 'load', function() { THIS._loaded(); });

    // Update form target.
    this.form.target = this._iframe_name;

    DebugConsole.debug(6, 'TempHiddenIFrame', "_createIFrame() finished.");
};

// Internal: delete iframe, if needed. This MUST be idempotent.
TempHiddenIFrame.prototype._removeIFrame = function(log)
{
    if (log) { DebugConsole.debug(7, 'TempHiddenIFrame', "_removeIFrame() called."); }

    // Remove iframe container.
    if (this._container) 
    { 
        document.body.removeChild(this._container);
        this._iframe = null;
        this._container = null;
    }

    if (log) { DebugConsole.debug(6, 'TempHiddenIFrame', "_removeIFrame() finished."); }
};

// Internal: callback that gets called when iframe loaded a page.
TempHiddenIFrame.prototype._loaded = function()
{
    DebugConsole.debug(7, 'TempHiddenIFrame', "_loaded() called.");

    // Get iframe document.
    if (this._iframe.contentDocument) { var doc = this._iframe.contentDocument; }
    else if (this._iframe.contentWindow) { var doc = this._iframe.contentWindow.document; }
    else { var doc = window.frames[this._iframe_name].document; }

    if (doc.location.href != "about:blank")
    {
        if (this.on_load)
        {
            // Call the on_load callback with the iframe contents as parameter.
            // Note: This is NOT reliable.
            try { this.on_load(doc.body.innerHTML); }
            catch(err) { } 
        }

        // Terminate usage.
        this.terminate();
    }

    DebugConsole.debug(6, 'TempHiddenIFrame', "_loaded() finished.");
};


