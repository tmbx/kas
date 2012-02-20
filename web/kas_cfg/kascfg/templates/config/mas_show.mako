## Template parameters
<%
    GT = c.GT
%>

<%inherit file="/layout.mako" />

<%def name="add_css()">
<link rel="stylesheet" href="/css/base/standard-forms-style.css" type="text/css" media="screen" />
<link rel="stylesheet" href="/css/kcd_cfg.css" type="text/css" media="screen" />
</%def>

<%def name="add_javascript()">
<script type="text/javascript" src="/javascripts/base/forms_help.js"></script>
</%def>

<h1>${GT("pages.mas_config.page_subtitle")}</h1>

<%include file="/common/_glob_messages.mako"/>

<h2>${GT("pages.basic_setup.page_subtitle")}</h2>

<!-- BASIC CONFIG FORM -->
<%include file="/standard_config_form.mako" args="store_name='kcd_basic_options'"/>


<!-- KFS FORM -->
<%include file="/standard_config_form.mako" args="store_name='kcd_kfs_options'"/>


<!-- ORG LIST FORM -->
<%include file="_kcd_org_list.mako"/>


<!-- INVISIBLE... GETS TRIGGERED WHEN USER ASKS FOR HELP (WHAT'S THIS) -->
<div id="helpbox">
    <div id="helpbox_message">
    </div>
    <div id="helpbox_close">
        <a href="javascript:void" onClick="javascript:formHelpHideHelp();">${GT("help.actions.close_help_window", "base")}</a>
    </div>
</div>



