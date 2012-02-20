<%inherit file="/layout.mako"/>

<script>
    function submitFunction(i){
        if (i==1)
            document.inviteForm.action="${url('credentials_download', workspace_id=c.workspace.id, email_id = c.email_id, interactive=1)}";
        else 
            document.inviteForm.action="${url('invite_create', workspace_id=c.workspace.id)}";
            
        document.inviteForm.submit();
    }
</script>


<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>

        <div class="main">
                <form name="inviteForm" method="post" action="${url('invite_create', workspace_id=c.workspace.id)}">
                    <input type="hidden" name="email_id" value="${c.email_id}"/>
                    %if c.show_pass:
                        <div style="padding-bottom:30px">Enter your password to access the secure Teambox.</div>
                        <label>Password</label>
                        <input type="password" name="password" />
                        <div style="padding-top:10px"></div>
                        <input type="button" name="logon" value="Web Access" onClick="submitFunction(0)"/>
                    %else:
                        <p style="padding-bottom:30px" >Download the credentials file (.wsl) to access the Teambox from you desktop via the Teambox Manager. </p>
                    %endif
                    <input type="button" name="download" value="Desktop Access" onClick="submitFunction(1)" />
                </form>

        </div>
        
    </div>
</div>
