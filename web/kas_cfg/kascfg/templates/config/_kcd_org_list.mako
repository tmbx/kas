## Template parameters
<%
    GT = c.GT
    ls = c.template_store['kcd_org_list']
%>

<% form = ls.forms.forms["kcd_org_list"] %>

## Anchor
<% local_action_anchor = "foid_" + form.id + "_start" %>
<% action_anchor = local_action_anchor %>
% if form.anchor:
  <% action_anchor = form.anchor %>
% endif
<a name="${local_action_anchor}"></a>

<div class="form">

<fieldset>
<legend>${GT("forms.kcd_org_list.legend_title")}</legend>

<div class="doc">
${GT("forms.kcd_org_list.doc")}
</div>

% if ls.error:
<div class="form_error">${ls.error}</div>
% endif

## Global notices
% if len(form.notices) > 0:
% for notice in form.notices:
<div class="form_notice">${notice}</div>
% endfor
% endif

## Global confirmations
% if len(form.confirmations) > 0:
% for confirmation in form.confirmations:
<div class="form_confirmation">${confirmation}</div>
% endfor
% endif



% if not form.read_only:
<div style="padding-left: 1em;">
<a href="${ls.action_url|n}?form=kcd_add_org#${action_anchor|n}">${GT("forms.actions.add", "base")}</a>
</div>
% endif

## edit organization sub-form
% if ls.kcd_edit_org_flag:
    <%include file="/standard_config_form.mako" args="store_name='kcd_edit_org'"/>
% endif

## add organization sub-form
% if ls.kcd_add_org_flag:
    <%include file="/standard_config_form.mako" args="store_name='kcd_add_org'"/>
% endif

<!-- <h2>${GT("forms.kcd_org_list.title")}</h2> -->

% if len(ls.organizations):
    <br />
    <table id="kcd_org_list" class="form">
    <tr>
        <td align="left" class="org_key_id_label">${GT("forms.kcd_org_list.org_key_id")}</td>
        <td align="left" class="org_name_label">${GT("forms.kcd_org_list.org_name")}</td>
        <td colspan="2">&nbsp;</td>
    </tr>
    % for k, v in ls.organizations.items():
    <tr>
        <td class="org_key_id">${k}</td>
        <td class="org_name">${v}</td>
        <td class="form_save action">
            % if not form.read_only:
            <form class="single_button" action="${ls.action_url|n}#${action_anchor|n}" method="post">
                <input name="form" type="hidden" value="kcd_edit_org" />
                <input name="org_key_id" type="hidden" value="${k}">
                <input type="submit" name="edit_key" class="form_edit" value="${GT("forms.actions.edit", "base")}">
            </form>
            % endif
        </td>
        <td class="form_save action">
            % if not form.read_only:
            <form class="single_button" action="${ls.action_url|n}#${action_anchor|n}" method="post">
                <input name="form" type="hidden" value="kcd_remove_org" />
                <input name="action" type="hidden" value="update" />
                <input name="org_key_id" type="hidden" value="${k}">
                <input type="submit" name="remove_key" class="form_delete" value="${GT("forms.actions.delete", "base")}" 
                    onClick="return confirm('${GT("forms.question.are_you_sure", "base")}')">
            </form>
            % endif
        </td>
    </tr>
    % endfor
    </table>
% else:
<div class="no_org_yet">${GT("forms.kcd_org_list.no_organization_yet")}</div>
% endif

</fieldset>

</div>

