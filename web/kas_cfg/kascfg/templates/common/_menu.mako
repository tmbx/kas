<div id="main-menu">
    <a class="menu-item" href="${h.url_for('status')}">${_('Dashboard')}</a>

    % if c.services['mas'].configured and c.services['mas'].enabled:
    <a class="menu-item" href="${h.url_for('teamboxes')}">${_('Manage Teamboxes')}</a>
    % else:
    <div class="menu-item">${_('Manage Teamboxes')}</div>
    % endif

    % if c.services['freemium'].configured and c.services['freemium'].enabled:
    <a class="menu-item" href="${h.url_for('users')}">${_('Manage users')}</a>
    % else:
    <div class="menu-item">${_('Manage users')}</div>
    % endif
</div>

