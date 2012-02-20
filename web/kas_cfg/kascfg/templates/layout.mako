<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" 
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
        
        <!--TODO: Temp workaround to force ie8 to render as IE7 to avoid modalbox problem with this doctype, 
            should fix the problem and remove this -->
        <meta http-equiv="X-UA-Compatible" content="IE=7" />

        <title>${_('Teambox Server Configuration')}</title>
        <link rel="stylesheet" href="/css/black.css" type="text/css" media="screen" />
        <%def name="add_css()">
        </%def>
        <%self.add_css()%>

 
        <script type="text/javascript" src="/javascripts/base/base.js"></script>
        <script type="text/javascript" src="/javascripts/base/prototype/prototype-1.6.0.3.js"></script>
        <script type="text/javascript" src="/javascripts/comm.js"></script>
        <%def name="add_javascript()">
        </%def>
        <%self.add_javascript()%>

        <%def name="set_initial_focus()">
	<script type="text/javascript">
	function set_initial_focus() { }
	</script>
        </%def>
        <%self.set_initial_focus()%>
    </head>

    <body onload="show_glob_messages_from_html_input(); set_initial_focus();">

    <%
        import simplejson
    %>
    <input type="hidden" id="glob_messages_input" value="${simplejson.dumps(map(lambda x: x.to_dict(), c.glob_messages))}" />

    <div id="container">

        <%include file="/common/_header.mako" />

        <div id="content">
            <div class="wrapper">


                <div class="side">

                    % if c.logged:
                    <%include file="/common/_menu.mako" />
                    % endif

                </div>

                <div class="main">

                    ${next.body()}

                </div>
            </div>
        </div>

        <%include file="/common/_footer.mako" />

    </div>

    </body>
</html>
