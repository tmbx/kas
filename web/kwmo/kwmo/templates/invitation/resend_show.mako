<%inherit file="/layout.mako"/>
<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>

        <div class="main">
            <div>Check your inbox for the invitation email. </div>
            <br/>
            <div> If you can't find the invitation email for this Teambox, provide your email address below to send you a new invitation.</div>
            <br/>
            <%include file="/invitation/_resend_form.partial"/>
        </div>
        
    </div>
</div>
