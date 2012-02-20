<%inherit file="/layout.mako"/>
<%def name="page_initalize()">
    <!--Team box javascript includes-->
    <script type="text/javascript" src="/javascripts/kajax.js?version=${c.dyn_ress_id}"></script>
    <script type="text/javascript" src="/javascripts/vnc.js?version=${c.dyn_version}"></script>
    <script type="text/javascript" src="/javascripts/ws.js?version=${c.dyn_version}"></script>
    <script type="text/javascript">function on_page_load(){kwmo_ws_init();}</script>
</%def>

<input type="hidden" id="kws_id" value="${c.workspace.id}" />
<input type="hidden" id="user_id" value="${c.user_id}" />
<input type="hidden" id="updater_state" value="${c.updater_state_json}" />

<div id="content">
    <div class="wrapper">
        <%include file="/common/glob_message.partial"/>
 
        <div class="main">
            <%include file="files.partial"/>
            
            <%include file="messages.partial"/>
        </div>
        
        <div class="side">
            <%include file="screen_sharing.partial"/>

            <%include file="members.partial"/>

            <%include file="pub.partial"/>
        </div>
    </div>
</div>
