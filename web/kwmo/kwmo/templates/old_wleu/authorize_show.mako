<%inherit file="/layout.mako"/>

<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>

        <div class="main">
                <form name="verify_pwd_form" method="POST" action="${url('verify_pwd_old_wleu', workspace_id=c.workspace.id)}">
                    <input type="hidden" name="email_id" value="${c.email_id}"/>
                    
                    <div style="padding-bottom:30px">Enter your password to access the secure Teambox.</div>
                    <label>Password:</label>
                    <input type="password" name="password" />
                    <div style="padding-top:10px"></div>
                    <input type="submit" name="logon" value="submit"/>

                </form>
                <br/>
                <small> If you were not given a password for this Teambox please <a href="#" onclick="Effect.toggle('resend_invitation_div', 'blind', { duration: 0.5 }); return false;">click here</a></small>
                
                <div style="display:none; margin:15px" id="resend_invitation_div">
                    <p style="margin-bottom:5px">Please enter your email address to send you a new invitation to this Teambox.</p>
                    <%include file="/invitation/_resend_form.partial"/>
                </div>

        </div>
        
    </div>
</div>
