<%
    import kwmo.lib.helpers as h
%>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
        
        <!--TODO: Temp workaround to force ie8 to render as IE7 to avoid modalbox problem with this doctype, should fix the problem and remove this -->
        <meta http-equiv="X-UA-Compatible" content="IE=7" />

        <title>Teambox Portal</title>
        <link href="/css/black.css" rel="stylesheet" type="text/css" />
        <link rel="stylesheet" href="/css/modalbox.css" type="text/css" media="screen" />
        <link type="text/css" href="/css/progressBar.css" rel="stylesheet" />

        <script type="text/javascript" src="/javascripts/base/prototype/prototype-1.6.0.3.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/base/prototype/scriptaculous.js?load=builder,effects"></script>
        <script type="text/javascript" src="/javascripts/base/base.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/base/debug_console.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/base/templates.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/base/tmp_hidden_iframe.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/kwmo.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/base/styleswitcher.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/base/modalbox.js?version=${c.dyn_version}"></script>
        <script type="text/javascript" src="/javascripts/comm.js?version=${c.dyn_version}"></script>
                
        <script type="text/javascript">
            // Fill this with urls that are needed in other javascript files.
            var kwmo_urls = {};
            %if c.base_url_paths:
                % for url_name, base_path in c.base_url_paths.items():
                    kwmo_urls['${url_name}'] = '${base_path}';
                % endfor
            %endif
        </script>

        <%def name="page_initalize()">
            <script type="text/javascript">function on_page_load(){}</script>
        </%def>
        <%self.page_initalize()%>

        <link rel="stylesheet" type="text/css" href="/css/black.css" title="Black" media="screen" />
        <link rel="alternate stylesheet" type="text/css" href="/css/blue.css" title="Blue" media="screen" />
        <link rel="alternate stylesheet" type="text/css" href="/css/orange.css" title="Orange" media="screen" />

    </head>

    <body onload="show_glob_messages_from_html_input(); on_page_load();">
        <%
            import simplejson
        %>
        <input type="hidden" id="glob_messages_input" value="${simplejson.dumps(map(lambda x: x.to_dict(), c.glob_messages))}" />
        <div id="container">
            <%include file="/common/header.partial"/>

            ${next.body()}
            
            <iframe id="ifr_hidden" name="ifr_hidden" width="0" height="0" frameborder="0" src="/html/blank.html">
                <!-- hide posts -->
            </iframe>

            <div id="debug_console" style="display:none">
                <div id="debug_content">
                </div>
            </div>
            
            <%include file="/common/footer.partial"/>
        </div>
    </body>
</html>

