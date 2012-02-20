<%inherit file="/layout.mako" />

<%
    # Map internal service names to public service names.
    map_service_name = {'mas' : 'MAS', 'wps' : 'WPS', 'tbsos' : 'TBSOS', 'freemium' : 'Freemium'}

    # Map services enabled status to a string.
    map_enabled_status = ['Disabled', 'Enabled']

    # Map config status to a string.
    map_config_status = ['Config incomplete', 'Config OK']

    # Map enabled status to a css class.
    map_enabled_status_class = ['disabled', 'enabled']

    # Map config status to a css class.
    map_config_status_class = ['incomplete', 'ok']

    # Display a submit button that can be enabled or disabled depending on a condition.
    def bool_submit(action, value, condition, cls="", method="POST"):
        if condition:
            return '<form action="'+action+'" method="'+method+'">' +\
                   '    <input class="'+cls+'" type="submit" value="'+value+'">' + \
                   '</form>'
        else:
            return '<input class="'+cls+'" type="submit" value="'+value+'" disabled>'
%>

<h1>Dashboard</h1>

<%include file="/common/_glob_messages.mako"/>

<div id="status" class="wrapper">

    ## Display a summary of all services.
    <table class="summary" cellpadding="0" cellspacing="0">
        % for service in c.services.values():
        <tr class="${map_enabled_status_class[service.enabled]} ${map_config_status_class[service.configured]}">
            <td class="service-name">
                ${map_service_name[service.name]}
            </td>
            <td class="enabled-status">
                ${map_enabled_status[service.enabled]}
            </td>
            <td class="config-status">
                ${map_config_status[service.configured]}
            </td>
            <td class="actions">
                <%
                    button_label = _('Configure')
                    if c.production_mode:
                        button_label = _('View configuration')
                %>
                ${bool_submit(h.url_for(service.name+'_config'), button_label, True, method="GET") |n}
            </td>
        </tr>
        % endfor
    </table>

    ## Display button for switching from/to production and maintenance modes.
    <div class="mode-buttons">
        ${bool_submit(h.url_for('switch_to_prod'), _('Production Mode'), True, 'prod', method="GET") |n}
        ${bool_submit(h.url_for('switch_to_maint'), _('Maintenance Mode'), True, 'maint', method="GET") |n}
    </div>

</div>

