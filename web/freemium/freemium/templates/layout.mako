<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
        
        <title>Teambox Portal ${("- " + c.page_title) if c.page_title else ""}</title>
        <link href="/freemium/css/black.css" rel="stylesheet" type="text/css" />
                
        <%def name="page_initalize()">
        </%def>
        <%self.page_initalize()%>

        <link rel="stylesheet" type="text/css" href="/freemium/css/black.css" title="Black" media="screen" />
        <link rel="alternate stylesheet" type="text/css" href="/freemium/css/blue.css" title="Blue" media="screen" />
        <link rel="alternate stylesheet" type="text/css" href="/freemium/css/orange.css" title="Orange" media="screen" />

        <script type="text/javascript" src="/freemium/javascripts/base/prototype/prototype-1.6.0.3.js"></script>
        <script type="text/javascript" src="/freemium/javascripts/base/styleswitcher.js"></script>

    </head>

    <body >

        <div id="container">
            <%include file="/common/header.partial"/>

            <div id="content">
                <div class="wrapper">
                    <div class="main">
                        ${next.body()}
                    </div>
                    
                    <div class="side">
                    </div>
                    
                </div>
            </div>
                    
                        
            <%include file="/common/footer.partial"/>
        </div>
    </body>
</html>
