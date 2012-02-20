<%inherit file="/layout.mako"/>

<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>

        <div class="main">
            <p>Please select your email address to access the Teambox:</p>
            %for member in c.same_pwd_members:
            <div><a href="#" onclick="submitForm(${member.user_id});return false;">${member.email}</a></div>
            %endfor
        </div>
        
    </div>
</div>

<!-- hidden html-->
<script>
    function submitForm(user_id){
            $('input_user_id').value = user_id;
            document.inviteForm.submit();
    }
</script>

<form name="inviteForm" style="display:none" method="POST" action="${url('select_member_old_wleu', workspace_id=c.workspace.id)}">
    <input type="hidden" name="user_id" id="input_user_id"/>
</form>

