## Template parameters
<%page args="store_name"/>

<%
    GT = c.GT
    ls = c.template_store[store_name]
    from kweb_forms import PasswordTextField
%>


    <% form = ls.forms.forms[ls.form_name] %>

    ## Make sure there are visible fields.. otherwise, do not show form
    <% show = False %>
    % for field in form.each_field():
        % if not field.hidden:
            <% show = True %>
        % endif
    % endfor

    ## Show the form only if wanted and if there are visible fields.
    % if form.show and show:

        ## Anchor
        <% local_action_anchor = "foid_" + form.id + "_start" %>
        <% action_anchor = local_action_anchor %>
        % if form.anchor:
            <% action_anchor = form.anchor %>
        % endif
        <a name="${local_action_anchor}"></a>

        <form id="foid_${form.id}" method="post" action="${ls.action_url|n}#${action_anchor|n}">
        ## Standard fields
        <input name="form" type="hidden" value="${form.id}" />
        <input name="action" type="hidden" value="update" />

        ## Hidden fields
        % for field in form.each_field():
        % if field.hidden:
        ${field.html_output()|n}
        % endif
        % endfor


        <div class="form">

        ## Required notice
        % if form.show_required_notice:
        <div class="form_required_notice">*&nbsp;${GT("forms.form.notice.required_field", "base")}</div>
        % endif

        % if form.show_legend:
        <fieldset>
        <legend>${GT("forms." + form.id + ".legend_title")}</legend>
        % endif

        % if form.show_title:
        <h2>${GT("forms." + form.id + ".legend_title")}</h2>
        % endif

        ## Global form error message
        % if form.filled and not form.valid():
        <div class="form_error">${GT("forms.form.error.there_are_errors", "base")}</div>
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


        <table class="form" border="0">


        ## Standard fields (not hidden)
        % for field in form.each_field():
        % if not field.hidden:

            ## Field errors
            % if form.filled and len(field.validation_exceptions) > 0:
            <tr>
            <td colspan="3">
                <div class="field_error">
                % for ex in field.validation_exceptions:
                    <p class="field_error_message">${str(ex)}</p>
                % endfor
                </div>
            </td>
            </tr>
            % endif

            ## Field line
            <tr>
            <td>
                <div class="field_part">
                    ## Field label
                    <div class="field_info">
                        <% req_notice = "" %>
                        % if field.required:
                            <% req_notice = "*" %>
                        % endif
                        <label for="${field.fid_html}">${req_notice}&nbsp;${GT("forms." + field.fid + ".label")}:</label>
                    </div>
                </div>
            </td>
            <td width="1">
                <div class="field_part">
                    ## Field input
                    <div class="field_input">
                        % if field.force_value or form.read_only:
                            <%
                                value = field.value
                                if isinstance(field, PasswordTextField) and value != '':
                                    value = '*******'
                            %>
                            ${value}
                            <input type="hidden" name="${field.id}" value="${value}" />
                        % else:
                            ${field.html_output()|n}
                        % endif
                    </div>
                </div>
            </td>
            <td width="40">
                <div class="field_part">
                    ## Whats's this link
                    <div class="field_help">
                        <a href="#" onclick="formHelpPopHelp(this,'dhelp_${field.fid_html}'); return false;">
                            ${GT("forms.field.help.label", "base", allow_basic_html=True)|n}
                        </a>
                        <div class="hidden" id="dhelp_${field.fid_html}">
                            ${GT("forms." + field.fid + ".whatsthis")}
                        </div>
                    </div>
                </div>
            </td>
            </tr>

        % endif
        % endfor

        % if not form.read_only:
        <tr>
            <td class="form_save" colspan="3">
                ## Form save and reset buttons
                <div class="form_save"><input name="commit" type="submit" value="${GT("forms.actions.save", "base")}" /></div>
                <div class="form_reset"><input type="reset" name="reset" value="${GT("forms.actions.reset", "base")}" /></div>
            </td>
        </tr>
        % endif

        </table>

        % if form.show_legend:
        </fieldset>
        % endif

        </div> <!-- class=form -->

        </form>

    % endif


