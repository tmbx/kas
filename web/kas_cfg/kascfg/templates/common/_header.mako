<div id="header">
    <div class="wrapper">
        <div class="teambox-logo">
            <a href="${h.url_for('teambox')}" target="_blank"></a>
        </div>

        <div class="header-main">
            <div class="header-logo">
                <h1>${_('Teambox Server Configuration')}</h1>
            </div>
        </div>

        <div class="header-user">
            <div class="header-user-name">
                <span>${_('Welcome!')}</span>

                % if c.logged:
                <a id="logout_link" href="${h.url_for('logout')}" class="button">${_('Logout')}</a>
                % endif
            </div>
        </div>
    </div>
    <hr />
</div>
