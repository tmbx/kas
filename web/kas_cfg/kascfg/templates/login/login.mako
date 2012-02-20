<%
    GT = c.GT
%>

<%inherit file="/layout.mako" />

<%def name="set_initial_focus()">
<script type="text/javascript">
function set_initial_focus() { $('password').focus(); }
</script>
</%def>


<%include file="/common/_glob_messages.mako"/>

<div id="login">

    <form class="login-form" action="${h.url_for('login')}" method="POST">
        Password: 
        <input type="password" id="password" name="cfg_password" value="${c.pwd}">
        <input type="submit" value="Login">
    </form>

</div>
