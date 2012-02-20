// KFS variables
var kfs_parent_dirs;
var kfs_dir;
var kfs_sub_dirs;
var uploadmod = null;

// Upload status possible values
var UPLOAD_STATUS_SUCCESS = 0
var UPLOAD_STATUS_ERROR = 1
var UPLOAD_STATUS_PENDING = 2

// Possible upload faliure reasons.
var UPLOAD_FAIL_GENERAL = 1
var UPLOAD_FAIL_LICENSE = 2
var UPLOAD_FAIL_ENCODING = 3
var UPLOAD_FAIL_FILE_EXTENSION = 4

// Start KWMO in Teambox mode.
function kwmo_ws_init()
{
    // Show logout link.
    $('logout_link').show();

    // init kwmo state
    init_kwmo();

    // Reset KWMO state.
    reset_kwmo_state();
    reset_kwmo_ws_state();

    // Reset VNC status.
    set_vnc_idle();

    // Instantiate upload module.
    uploadmod = new UploadModule();
    uploadmod.init();

    // Extract the templates.
    dom_templates_extract('file_upload_template', 'file_upload');

    // Add state handlers. Defaults state handlers are set in kwmo.js.
    state_handlers[state_handlers.length] = new Array(STATE_WANT_PERMS, ws_update_perms);
    state_handlers[state_handlers.length] = new Array(STATE_WANT_WS, update_ws);
    state_handlers[state_handlers.length] = new Array(STATE_WANT_MEMBERS, update_members);
    state_handlers[state_handlers.length] = new Array(STATE_WANT_CHAT, ChatWidget.update_messages);
    state_handlers[state_handlers.length] = new Array(STATE_WANT_KFS, update_kfs);
    state_handlers[state_handlers.length] = new Array(STATE_WANT_VNC, update_vnc_sessions);
    state_handlers[state_handlers.length] = new Array(STATE_WANT_UPLOAD_STATUS, function(res) { uploadmod.updateUploadStatus(res); });

    // Set state updater parameters.
    state_add_cur_flags(STATE_WANT_PERMS | STATE_WANT_WS | STATE_WANT_MEMBERS | STATE_WANT_VNC | STATE_WANT_KFS);

    // Initialize chat and chat POST widgets.
    ChatWidget.init();
    ChatPOSTWidget.init();

    // Get first state from html, and start the update loop.
    state_update_from_html_input();
}


// Update perms.
function ws_update_perms(result)
{
    update_perms(result);
    var enable = false;
    if (perms.hasPerm('kfs.upload.share.' + kfs_share_id)) { enable = true; }
    if (enable)
    {
        DebugConsole.debug(5, null, "Enabling file upload.");
        uploadmod.permEnable();
    }
    else
    {
        DebugConsole.debug(5, null, "Disabling file upload.");
        uploadmod.permDisable();
    }

    if (perms.hasPerm('vnc.list'))
    {
        DebugConsole.debug(5, null, "Showing shared applications block.");
        $('shared_app_block').show();
    }
    else
    {
        DebugConsole.debug(5, null, "Hiding shared applications block.");
        $('shared_app_block').hide();
    }
}

// This function resets the KWMO initial state.
function reset_kwmo_ws_state()
{
    // Get current kfs directory from an hidden html input node.
    kfs_dir = $('kfs_dir').value.evalJSON(sanitize = true);

    // Set kfs dir in the state update parameters.
    state_add_cur_params({"kfs_dir":kfs_dir});
    kfs_parent_dirs = [kfs_dir];
    kfs_sub_dirs = [];
}

// This function is run on the first update.
function kwmo_first_update()
{
    DebugConsole.debug(7, null, "kwmo_first_update() called.");
    DebugConsole.debug(6, null, "kwmo_first_update() finished.");
}

var UPLOAD_MOD_IDLE = 1;
var UPLOAD_MOD_BROWSING = 2;
var UPLOAD_MOD_UPLOADING = 3;

// Upload widget
var UploadModule = function()
{
    // Module status.
    this.status = UPLOAD_MOD_IDLE;

    // Flag: do we have the upload perm?
    this.uploadPerm = false;

    // External random upload id.
    this.externalUploadRandomID = null;

    // Upload form.
    this.uploadForm = null;

    // Temporary hidden iframe object.
    this.thf = null;
};

// Initialize widget.
UploadModule.prototype.init = function()
{
    // Initialize temporary hidden iframe object.
    this.thf = new TempHiddenIFrame();

    // Reset.
    this._reset();

};

// Enable upload module permission.
UploadModule.prototype.permEnable = function()
{
    DebugConsole.debug(4, null, "Enabling upload module permission.");
    this.uploadPerm = true;
    this.conditionallyEnableUploadLinks();
};

// Disable upload module permission.
UploadModule.prototype.permDisable = function()
{
    DebugConsole.debug(4, null, "Disabling upload module permission.");

    this.uploadPerm = false;
    this._reset();
    this.disableUploadLinks();
};

// Conditionally enable upload links.
UploadModule.prototype.conditionallyEnableUploadLinks = function()
{
    if (this.status == UPLOAD_MOD_IDLE && this.uploadPerm) { this.enableUploadLinks(); }
    else { this.disableUploadLinks(); }
}

// Enable upload links.
UploadModule.prototype.enableUploadLinks = function()
{
    // Enable file update buttons.
    var l = $('files_list').select('.file-update');
    var elem;
    for (var i=0; i<l.length; i++)
    {
        elem = l[i];
        elem.removeClassName('disabled');
        elem.disabled = false;
        elem.show();
    }

    // Update view.
    this.updateView();
};

// Disable upload links.
UploadModule.prototype.disableUploadLinks = function()
{
    // Disable file update buttons.
    var l = $('files_list').select('.file-update');
    var elem;
    for (var i=0; i<l.length; i++)
    {
        elem = l[i];
        elem.addClassName('disabled');
        elem.disabled = true;
    }

    // Update view.
    this.updateView();
};

// User action: upload link was clicked.
UploadModule.prototype.uploadLinkClicked = function(inode_id, commit_id)
{

    // Ignore click if upload links are disabled.
    if (this.status != UPLOAD_MOD_IDLE || !this.uploadPerm) 
    { 
        DebugConsole.debug(1, null, "Ignoring upload link click"); 
        return;
    }

    // Entering browsing mode.
    this.status = UPLOAD_MOD_BROWSING;

    DebugConsole.debug(5, null, "Showing file upload form.");

    // Disable upload links.
    this.disableUploadLinks();

    // Prepare upload form.
    this.prepareUploadForm(inode_id, commit_id);

    DebugConsole.debug(4, null, "Showed file upload form.");
};

// User action: cancel link was clicked.
UploadModule.prototype.cancelLinkClicked = function()
{
    DebugConsole.debug(5, null, "Cancelling file upload.");
 
    // Reset module.
    this._reset();

    DebugConsole.debug(4, null, "Cancelled file upload.");
};

// Prepare upload form.
UploadModule.prototype.prepareUploadForm = function(inode_id, commit_id)
{
    var obj, obj_parent;

    var THIS = this;

    // Get the container.
    obj_parent = get_by_id("file_upload_container")

    // Get the template.
    obj = dom_templates_get('file_upload');

    // Add obj in it's container.
    obj_parent.appendChild(obj);

    // Set upload form.
    this.uploadForm = $('form_upload');

    // Init temporary hidden iframe object.
    this.thf.prepare(this.uploadForm, null, function() { THIS.uploadStarted(); }, function(data) { THIS.iframeLoaded(data); });

    // Add an onSubmit event to the form (some browsers do not clone event listeners when cloning elements).
    // (templates use the cloneNone() function).
    if (this.uploadForm.addEventListener !== undefined)
    {
        this.uploadForm.addEventListener("submit", function() { return THIS.submitButtonClicked(inode_id, commit_id); }, false);
    }
    else if(typeof(document.attachEvent)!==undefined)
    {
        this.uploadForm.attachEvent("onsubmit", function() { return THIS.submitButtonClicked(inode_id, commit_id); });
    }

    // Update view.
    this.updateView();
};

// Update view.
UploadModule.prototype.updateView = function()
{
    if (this.uploadPerm && this.status == UPLOAD_MOD_BROWSING)
    {
        // Show upload form.
        $('file_upload_container').show();
    }
    else
    {
        // Hide upload form.
        $('file_upload_container').hide();
    }
    if (this.uploadPerm)
    {
        if (this.status == UPLOAD_MOD_IDLE)
        {
            // Hide "add" nolink.
            $('file_upload_nolink').hide();

            // Hide "cancel" link.
            $('file_upload_cancel_link').hide();

            // Show "add" link.
            $('file_upload_link').show();
        }
        else
        {
            // Hide "add" link.
            $('file_upload_link').hide();

            // Hide "add" nolink.
            $('file_upload_nolink').hide();

            // Show "cancel" link.
            $('file_upload_cancel_link').show();
        }
    }
    else
    {
        // Hide "add" link.
        $('file_upload_link').hide();

        // Hide "cancel" link.
        $('file_upload_cancel_link').hide();

        // Show "add" nolink.
        var disabled_reason = 'File upload is disabled';
        if (perms && perms.hasRole('freeze')) { disabled_reason = 'Workspace is moderated.'; }
        $('file_upload_nolink').title = disabled_reason;
        $('file_upload_nolink').show();
    }

    $('file_upload_link_container').show();
    $('file_upload_wrap_container').show();
};

// User action: user has clicked on submit.
UploadModule.prototype.submitButtonClicked = function(inode_id, commit_id)
{
    var THIS = this;

    if ($('file_upload_file').value == "")
    {
        // Do not submit when user has not selected a file to upload. 
        return false;
    }

    // Complete form intormations before posting.
    this.completeForm(inode_id, commit_id);

    return this.thf.submit();
};

// Complete the form before file is submitted.
UploadModule.prototype.completeForm = function(inode_id, commit_id)
{
    // Get a random upload ID.
    this.externalUploadRandomID = Math.floor(Math.random() * 9999999);

    // Set periodical updater hooks.
    state_add_cur_flags(STATE_WANT_UPLOAD_STATUS);
    state_add_cur_params({"last_upload_random_id":this.externalUploadRandomID});

    // Set form action. 
    if (inode_id && commit_id)
    {
        // Don't use commit_id (for now at least). //  commit_id:commit_id,
        this.uploadForm.action = get_url_path('teambox_upload') + "/" + kws_id + "?" +
            Object.toQueryString({inode_id:inode_id, client_random_id:this.externalUploadRandomID})
    }
    else
    {
        this.uploadForm.action = get_url_path('teambox_upload') + "/" + kws_id + "?" +
            Object.toQueryString({kfs_dir:Object.toJSON(kfs_dir), 
                                  client_random_id:this.externalUploadRandomID});
    }
   
    DebugConsole.debug(5, null, "File upload prepared.");
};

// Callback: upload is started.
UploadModule.prototype.uploadStarted = function()
{
    DebugConsole.debug(4, null, "File upload started.");

    // Change status.
    this.status = UPLOAD_MOD_UPLOADING;

    // Update view.
    this.updateView();

    // Show pending status.
    $('file_upload_status_pending').show();
};

// Callback: upload is terminated.
UploadModule.prototype.iframeLoaded = function(data)
{
    DebugConsole.debug(1, null, "Upload hidden iframe loaded:" + data);
};

// Upload is aborted.
UploadModule.prototype.abortUpload = function(reason)
{
    DebugConsole.debug(5, null, "Aborting file upload: "+reason);

    GlobalMessage.error(reason);

    // Reset module.
    this._reset();

    DebugConsole.debug(5, null, "Aborted file upload.");
};

// Upload is finished.
UploadModule.prototype.finishUpload = function()
{
    DebugConsole.debug(5, null, "Finishing file upload.");

    GlobalMessage.info("File upload is complete.");

    // Reset module.
    this._reset();

    DebugConsole.debug(5, null, "Finished file upload.");
};

// Callback: called by the periodical updater to update file upload status.
UploadModule.prototype.updateUploadStatus = function(res)
{
    var all_files_upload_status = res["data"];

    // Support for multiple files can be added
    var file_upload_status = all_files_upload_status[this.externalUploadRandomID];
    var status = file_upload_status['status'];
    var failure_reason = file_upload_status['failure_reason'];

    if (status != UPLOAD_STATUS_PENDING)
    {
        if (status == UPLOAD_STATUS_SUCCESS)
        {
            // Terminate upload.
            this.finishUpload();
        }
        else if (status == UPLOAD_STATUS_ERROR && failure_reason == UPLOAD_FAIL_LICENSE)
        {
            // Abort upload.
            this.abortUpload("File upload failed: Not enough space left on this Teambox. Contact the owner.");
        }
        else if (status == UPLOAD_STATUS_ERROR && failure_reason == UPLOAD_FAIL_ENCODING)
        {
            // Abort upload.
            this.abortUpload("File upload failed: Invalid file name.");
        }
        else if (status == UPLOAD_STATUS_ERROR && failure_reason == UPLOAD_FAIL_FILE_EXTENSION)
        {
            // Abort upload.
            this.abortUpload("File upload failed: Bad file extension.");
        }
        else
        {
            // Abort upload.
            this.abortUpload("File upload failed.");
        }
    }
};

// Internal: reset upload module.
UploadModule.prototype._reset = function()
{
    if (this.externalUploadRandomID != null)
    {
        // Remove periodical updater hooks.
        state_del_cur_flags(STATE_WANT_UPLOAD_STATUS);
        state_del_cur_params(new Array("last_upload_random_id"));
        state_restart_loop();
        this.externalUploadRandomID = null;
    }

    // Remove form, if needed.
    try { remove_dom_id("file_upload"); }
    catch(e) { }
    this.uploadForm = null;

    // Hide pending status, if needed.
    $('file_upload_status_pending').hide();

    // Terminate temporary hidden iframe usage, if needed.
    this.thf.terminate();

    // Set status to idle.
    this.status = UPLOAD_MOD_IDLE;

    // Conditionally enable links.
    this.conditionallyEnableUploadLinks();
};

// This function wraps the functions updating the KFS widget.
function update_kfs(res)
{
    DebugConsole.debug(7, null, "update_kfs() called");

    var evt_id = res["last_evt"];
    var kfs_status = res["data"];

    kfs_parent_dirs = kfs_status.components;
    set_content_by_id("cur_path", convert_components_name_to_string(kfs_status.components));

    kfs_dir = kfs_parent_dirs[kfs_parent_dirs.length-1];
    DebugConsole.debug(9, null, "Current KFS directory: " + dump(kfs_dir));
    update_kfs_dir_list(kfs_status.dirs);
    update_kfs_file_list(kfs_status.files);

    var obj = get_by_id("files_list");
    if (obj && obj.innerHTML.empty())
    {
        obj.innerHTML = get_by_id('empty_dir_template_container').innerHTML;
    }

    state_add_cur_params({"last_evt_kfs_id":evt_id});
}

// This function converts components name into a string.
function convert_components_name_to_string(components)
{
    var s = '';
    if (kfs_parent_dirs.length > 1)
    {
        get_by_id('up_dir_link').onclick = function() {kfs_backward(); return false;};
    }
    else
    {
        get_by_id('up_dir_link').onclick = function() {return false;};
    }


    s = s + ' <a href="#" onclick="javascript:kfs_go_to_parent_dir(' + 0 +'); return false;">HOME</a> / ';
    for (var i=1; i<components.length; i++)
    {
        s = s + '<a href="#" onclick="javascript:kfs_go_to_parent_dir(' + i +'); return false;">' + components[i].name + '</a>' + ' / ';
    }
    return s;
}

// This function updates the KFS files list.
function update_kfs_file_list(list)
{
    //This function assumes the file_list html has already been filled with directories. So it appends the html files list after constructing it.
    //TODO: to be replace with JST

    DebugConsole.debug(7, null, "update_kfs_file_list() called");

    var str = '';
    var fileOKTemplate = get_by_id('file_ok_template_container').innerHTML;
    var fileUploadingTemplate = get_by_id('file_uploading_template_container').innerHTML;

    var ordered_names = list_map(list, function(x) { return x.name; }).sort(function(a,b) { return a.toLowerCase() > b.toLowerCase(); });
    var nodes_hash = hash_map(list, function(x) { return [x.name, x]; });
    var f, tmpStr;
    for (var i=0; i<ordered_names.length; i++)
    {
        f = nodes_hash[ordered_names[i]];
        if (f.status == 1) { tmpStr = fileOKTemplate; }
        else { tmpStr = fileUploadingTemplate; }

        tmpStr = tmpStr.replace("var_file_url", kfs_download_open_url(f));
        tmpStr = tmpStr.replace("var_file_name", replace_nbsp(f.esc_name));
        tmpStr = tmpStr.replace("var_file_size", replace_nbsp(kfs_convert_file_size(f.file_size)));
        tmpStr = tmpStr.replace("var_file_user", replace_nbsp(get_user_or_email(f.user_id)));
        tmpStr = tmpStr.replace("var_file_date_time", replace_nbsp(format_date_iso(f.mdate, false)));
        tmpStr = tmpStr.replace("var_file_inode_id", f.inode_id);
        tmpStr = tmpStr.replace("var_file_commit_id", f.commit_id);
        str+= tmpStr;
    }

    var obj = get_by_id("files_list");
    if (obj) { obj.innerHTML += str; }

    // FIXME reset permissions every time... this needs to be rewritten someway.
    uploadmod.conditionallyEnableUploadLinks();
}

// This function updates the KFS directories list.
function update_kfs_dir_list(list)
{
    DebugConsole.debug(7, null, "update_kfs_dir_list() called");

    // Update global sub-directories list.
    kfs_sub_dirs = list;

    //TODO: to be replaced by JST
    var dir_template = get_by_id('dir_template_container').innerHTML;
    var str = '';

    // FIX navigated correctly through ordered subdirs
    // TODO: optimize the following javascript code, too many iterations.
    var indexed_list = []
    for (var i=0; i<list.length; i++)
    {
        indexed_list[i] = [list[i], i];
    }

    var ordered_names = list_map(list, function(x) { return x.name; }).sort(function(a,b) { return a.toLowerCase() > b.toLowerCase(); });
    var nodes_hash = hash_map(indexed_list, function(x) { return [x[0].name, x]; });
    var d, tmpStr;
    for (var i=0; i<ordered_names.length; i++)
    {
        d = nodes_hash[ordered_names[i]][0];
        tmpStr = dir_template;

        tmpStr = tmpStr.replace("var_sub_dir_no", nodes_hash[ordered_names[i]][1]);
        tmpStr = tmpStr.replace("var_dir_name",  replace_nbsp(d.esc_name));

        tmpStr = tmpStr.replace("var_file_count", d.files_count);
        tmpStr = tmpStr.replace("var_subdir_count", d.subdirs_count);
        str += tmpStr;
    }
    var obj = get_by_id("files_list");
    if (obj)
    {
        obj.innerHTML = str;
    }
}

// This function sends a request to go back in KFS path.
function kfs_backward()
{
    state_add_one_time_flags(STATE_FORCE_SYNC);
    tmp_kfs_dir = kfs_parent_dirs[kfs_parent_dirs.length-2];
    if (tmp_kfs_dir)
    {
        state_add_cur_params({"kfs_dir":tmp_kfs_dir});
    }
    else
    {
        DebugConsole.debug("No parent dir found... sending in root dir.");
        state_del_cur_params(new Array("kfs_dir"));
    }
    state_restart_loop(STATE_DELAY_NONE);
}

// This function sends a request to change KFS directory.
function kfs_go_to_sub_dir(idx)
{
    if (kfs_sub_dirs[idx]){
        state_add_one_time_flags(STATE_FORCE_SYNC);
        state_add_cur_params({"kfs_dir":kfs_sub_dirs[idx]});
        state_restart_loop(STATE_DELAY_NONE);
    }
}

function kfs_go_to_parent_dir(idx)
{
    if (kfs_parent_dirs[idx]){
        state_add_one_time_flags(STATE_FORCE_SYNC);
        state_add_cur_params({"kfs_dir":kfs_parent_dirs[idx]});
        state_restart_loop(STATE_DELAY_NONE);
    }
}

