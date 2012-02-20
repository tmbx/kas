<%inherit file="/layout.mako"/>
<%def name="page_initalize()">
    <!--javascript includes-->
</%def>

<div class="msg">
%if c.success:
    <div>Your email address has been confirmed. You can now go back to your Teambox Manager and click Next.</div>
    <br><br>
    <img height="350" src="/freemium/images/screenshots/EmailConfScreenshot.png"/>
%else:
    <div>Oops! We were unable to confirm your email address.</div>
    <br>
    <h4>Possible reasons:</h4>
    <ul style="margin-left:30px;">
        <li>Your email address has already been confirmed.</li>
        <li>The link you used is invalid. Make sure to copy and paste the entire link in your web browser and try again.</li>
    </ul>
%endif
</div>
