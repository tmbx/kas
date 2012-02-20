<%inherit file="/layout.mako"/>

<%def name="page_initalize()">

        <script type="text/javascript" src="/javascripts/kajax.js?version=${c.dyn_ress_id}"></script>
        <script type="text/javascript" src="/javascripts/pubws.js?version=${c.dyn_ress_id}"></script>
        <script type="text/javascript" src="/javascripts/base/ProgressBar-1.0.1.js"></script>

        <script type="text/javascript">
            add_onload_event("kwmo_pubws_init();");
            function on_page_load(){run_onload_events();}
        </script>

</%def>

<input type="hidden" id="kws_id" value="${c.workspace.id}" />
<input type="hidden" id="user_id" value="${c.user_id or ''}" />
<input type="hidden" id="updater_state" value="${c.updater_state_json}" />
<input type="hidden" id="pubws_email_info" value="${c.json_email_info_str}" />
<input type="hidden" id="pubws_identities" value="${c.json_identities_str}" />

<%!
    import time

    def name_address(identity):
        if identity['name'] and identity['name'] != '' and identity['name'] != identity['email']:
            return "%s (%s)" % ( identity['name'], identity['email'] )
        elif identity['email']:
            return identity['email']
        else:
            return "[Unknown User]"
    
    def name_or_address(identity):
        if identity['name'] and identity['name'] != '':
            return identity['name']
        elif identity['email']:
            return identity['email']
        else:
            return "[Unknown User]"
%>


<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>

        <div class="main">
            <!-- Email Information Block -->
            <div class="block block-email-info">
                <div class="block-header">
                    <h2>Email Information:</h2>
                </div>
                <div class="block-content">
                    <div id="members_list" class="block-wrapper">

                        <div id="mail_info" class="email_information">

                            <dl><dt>Subject: <span>${c.email_info['subject']}</span></dt></dl>
                            <dl><dt>Date: <span>${time.strftime('%Y-%m-%d %H:%S', time.localtime(c.email_info['date']))}</span></dt></dl>
                            
                            <dl id="Sender_container">
                                <dt>From:</dt>
                                    <dd class="identity"><strong>${name_address(c.identities[0])}</strong><span id="view_files_0" style="display:none"> - <a href="#" onclick="Effect.toggle('file_list_0', 'appear'); return false;" title="Files which were not originally attached to this email, that were dropped by user later on.">show files</a></span></dd>
                                    <dd class="file_list" id="file_list_0" style="display:none"></dd>
                            </dl>

                            <dl id="all_identities_list">
                                <dt>To:</dt>
                                <% i = 1 %>
                                % if len(c.identities) == 1:
                                    <dd class="identity"><strong>${name_address(c.identities[0])}</strong></dd>
                                % else:
                                % for r in c.identities[1:]:
                                    <dd class="identity"><strong>${name_address(r)}</strong><span id="view_files_${i}" style="display:none"> - <a href="#" onclick="Effect.toggle('file_list_${i}', 'appear'); return false;" title="Files which were not originally attached to this email, that were dropped by user later on.">show files</a></span></dd>
                                    <dd class="file_list" id="file_list_${i}" style="display:none"></dd>
                                    <% i += 1 %>
                              
                                % endfor
                                % endif
                            </dl>
                            
                        </div>
                        
                    </div>
                </div>
            </div>


            <noscript id="noscript">
                <!-- visible only when browser does not support javascript -->
                <h1>You need to enable Javascript in your browser to use this page.</h1>
            </noscript>

            <!-- Attachments Block -->
            <div id="attachments_block" class="block" style="display: none;">
                <div class="block-header">
                    <h2>Attachments:</h2>
                </div>
                <div class="block-content">
                    <div id="attachment_info" class="block-wrapper">
                        
                    </div>
                    <div id="attachment_list" class="block-wrapper">
                        
                    </div>
                    <div class="block-wrapper">
                        <div id="attachments_chat" style="display: none">
                            ## WARNING: don't separate span and a tags because firstChild is used in javascript.
                            <div id="att_chat_link"><a href="#" onclick="return false;">Chat now</a> with var_sender for more information.
                            </div>
                            <div id="att_chat_nolink" style="display: none;">Chat now with var_sender for more information.</div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Chat Block -->
            <%include file="messages.partial"/>

        </div>
        

        <div class="side">
        
            <!-- Actions Block -->
            <div class="block action_links">
                <div class="block-header">
                    <h2>Do More</h2>
                </div>
                
                <div class="block-content">
                    <div class="block-wrapper">
                        <div style="margin-top:10px; margin-left:10px;"><strong>What would you like to do with ${name_or_address(c.identities[0])}:</strong></div>
                        
                        <ul id="action_links" style="display: none;">
                            <li id="chat_link_container">
                                <span id="chat_link"><a href="#" onclick="return false;">Chat live</a></span>
                                <span id="chat_nolink" style="display: none;">Chat live</span>
                            </li>
                            <li>
                                <span id="send_file_link"><a href="#" onclick="return false;">Drop a file (up to 50MB)</a></span>
                                <span id="send_file_nolink" style="display: none;">Drop a file (up to 50MB)</span>

                                    <!-- kfs file upload widget -->
                                    <div id="file_upload_container" >
                                    </div>
                            </li>
                            <li>
                                <span id="wscreate_link"><a href="#" onclick="return false;">Share a permanent Teambox</a></span>
                                <span id="wscreate_nolink" style="display: none;">Share a permanent Teambox</span>

                                <a style="margin-left:5px;" id="wscreate_learn" href="#" onclick="Effect.toggle('wscreate_learn_tip', 'blind'); return false;"><img src="/images/graphics/common/icons/help-browser.png"></img></a>

                                <div id="wscreate_learn_tip" style="display:none">
                                    <strong>A permanent Teambox would allow you to:</strong>
                                    <ul>
                                        <li>Securely Chat</li>
                                        <li class="highlight">Securely Share files and directories</li>
                                        <li class="highlight">Securely Share screens</li>
                                        <!--<li><a href="http://www.teambox.co">Learn more on Teambox&#39;s web site</a></li>-->
                                    </ul>
                                </div>
                            </li>
                        </ul>

                    </div>
                </div>
            </div>
        
        </div>
    </div>
</div>


<!-- identity selection widget -->
<div id="select_identity_container">
    <div id="select_identity">
    </div>
</div>



<!--hidden Templates HTML -->

<div id="attachments_template_expiring" style="display:none">
    <div>Files will be available until var_expire_date.</div>
</div>

<div id="attachments_template_expired" style="display:none">
    <div>Files attached to this email are no longer available.</div>
</div>

<div id="attached_file_template" style="display: none;">
    <div class="attachment-file"><a href="#" onclick="return pubws_kfsdown.click('var_file_email', var_file_idx);">var_file_name</a> <small>var_file_size</small></div>
</div>

<div id="attached_file_template_expired" style="display: none;">
    <div class="attachment-file"><span>var_file_name</span> <small></small></div>
</div>

<div id="attached_file_template_pending" style="display: none;">
    <div class="attachment-file"><span>var_file_name</span> <small> (pending)</small></div>
</div>

<div id="attached_file_template_deleted" style="display: none;">
    <div class="attachment-file"><span>var_file_name</span> <small> (deleted by sender)</small></div>
</div>

<div id="attachment_list_pending_template" style="display:none">
    <div class="app-alert loading">Upload still in progess...</div>
</div>

<div id="file_upload_template" style="display: none;">
    <form id="form_upload" target="ifr_hidden" method="post"
        enctype="multipart/form-data">
        <div id="file_upload_field" class="file_upload_field">
            <div class="file_upload_label">
                File: 
            </div>
            <input id="file_upload_input" size="15" type="file" name="file_1" /> <!-- replaced in javascript -->
        </div>

        <div class="dialog_send">
            <button type="submit">Upload</button>
            <button id="file_upload_cancel" type="button" onclick="pubws_kfsup.hideFileUploadWidget()">Cancel</button>

        </div>
    </form>
</div>

