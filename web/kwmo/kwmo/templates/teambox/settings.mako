<%inherit file="/layout.mako"/>

<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>

        <div class="main">
            <h3>Customize your Teambox settings:</h3>
            
            <form name="settingsForm" method="POST" action="${url('teambox_settings_update', workspace_id=c.workspace.id)}" class="settings">
                <fieldset>
                    <legend><b>Email notifications</b></legend>
                    <div>
                        <input type="checkbox" class="chk" id="notification_checkbox" name="notification" value="1" ${'checked' if c.notificationEnabled else ''}/>
                        <label for="notification_checkbox">Send event notification emails for this Teambox.</label>
                    </div>
                    %if not c.workspace.public:
                        <div>
                            <input type="checkbox" class="chk" id="summary_checkbox" name="summary" value="1" ${'checked' if c.summaryEnabled else ''}/>
                            <label for="summary_checkbox">Send summary emails for this Teambox. </label>
                        </div>
                    %endif
                </fieldset>

                <!--
                <fieldset>
                    <legend><b>Other settings</b></legend>
                    <div>
                        <input type="checkbox" class="chk" id="notification_checkbox" name="notification" value="1" />
                        <label for="notification_checkbox">Send event notification emails for this teambox</label>
                    </div>
                    <div>
                        <input type="checkbox" class="chk" id="summary_checkbox" name="summary" value="1" />
                        <label for="summary_checkbox">Send summary emails for this teambox </label>
                    </div>
                    
                </fieldset>
                -->

                <a class="button" href="#" onclick="document.settingsForm.submit();return false;">apply</a>
                <a class="button" href="${c.cancel_url}">cancel<a/>
            </form>

        </div>
        
    </div>
</div>
