<%inherit file="/layout.mako"/>
<%def name="page_initalize()">
    <!--javascript includes-->
</%def>
<%
    import time
%>
<h1>User management</h1>

<%include file="/common/_glob_messages.mako"/>

<fieldset style="padding:10px;">
    <legend style="margin-left:15px;">Manage registered users</legend>
    % if c.users_count >0:
        <div style="text-align:right"><b><i>Matching users: </i>${c.users_count}</b></div>
        <form method="POST" action="${url('manage_users_apply')}">
            <div id="user_list" class="users-list">
                <div class="users-list-header">
                    <div class="email"><a href="${url('manage_users', limit = c.limit, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = 'email', order_dir='desc' if c.order_dir=='asc' else 'asc')}">User email</a></div>
                    <div class="license"><a href="${url('manage_users', limit = c.limit, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = 'license', order_dir='desc' if c.order_dir=='asc' else 'asc')}">License</a></div>
                    <div class="password">Password</div>
                    <div class="created_on"><a href="${url('manage_users', limit = c.limit, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = 'created', order_dir='desc' if c.order_dir=='asc' else 'asc')}">Created</a></div>
                    <div class="notes">Note</div>
                </div>

            % for user in c.users:
                <div class="users-list-item">
                    <input type="checkbox" name="user_record_${user.id}" value="${user.id}"/>
                    <div class="email">${user.email}</div>
                    <div class="license">${user.license}</div>
                    <div class="password pwd-text">${user.pwd}</div>
                    <div class="password pwd-stars">********</div>
                    <div class="created_on">${time.strftime("%Y-%m-%d", time.localtime(user.created_on)) if (user.created_on) else 'N/A' }</div>
                    <div class="notes">${user.note}</div>
                </div>
            % endfor
            </div>
            <br/>

            <div style="margin-bottom:5px; margin-top:5px; font-size:10px; clear:both;">
                Select <a href="#" onclick="$$('.users-list-item input').each(function(uu){uu.checked=true;}); return false;">All</a> <a href="#" onclick="$$('.users-list-item input').each(function(uu){uu.checked=false;}); return false;">None</a>
            </div>
            <div style="margin-bottom:20px; font-size:10px;">
                <input type="checkbox" id="input_show_pass" onclick="if (this.checked){$('user_list').addClassName('show-password');} else {$('user_list').removeClassName('show-password');} "/>
                <label for="input_show_pass">Show passwords</label>
            </div>

            

            <div>
                <script type="text/javascript">
                    function on_apply_action_change(selectElement)
                    {
                        $('input_set_password').hide();
                        $('input_set_note').hide();

                        if (selectElement.options[selectElement.selectedIndex].value=='set_password') 
                        {
                            $('input_set_password').show();
                        }
                        else if (selectElement.options[selectElement.selectedIndex].value=='set_note') 
                        {
                            $('input_set_note').show();
                        }
                        
                    }
                </script>
                
                <select name="apply_action" onchange = "on_apply_action_change(this);">
                    <option value="select_action" selected="selected">[Select Action]</option>
                    <option value="set_password">Set password</option>
                    <option value="set_note">Set Note</option>
                    <option value="synchronize">Re-synchronize</option>
                </select>
                            
                <input type="password" style="display:none" id="input_set_password" name="password"/>
                <input type="text" style="display:none" id="input_set_note" name="note"/>
                <input type="hidden" name="limit" value="${c.limit}"/>
                <input type="hidden" name="page" value="${c.page}"/>
                <input type="hidden" name="order_by" value="${c.order_by}"/>
                <input type="hidden" name="order_dir" value="${c.order_dir}"/>
                <input type="hidden" name="license_criteria" value="${c.license_criteria}"/>
                <input type="hidden" name="email_criteria" value="${c.email_criteria}"/>
                <input type="submit" value="apply"/>
            </div>
            <div style="margin-top:10px;">
                <select name="license_action">
                    <option value="license" selected="selected">[Select License]</option>
                    <option value="none">No license</option>
                    <option value="freemium">Set Freemium license</option>
                    <option value="bronze">Set Bronze license</option>
                    <option value="silver">Set Silver license</option>
                    <option value="gold">Set Gold license</option>
                </select>
                <input type="submit" value="apply"/>
            </div>
        </form>
    % else:
        <i>No users found.</i>
    % endif:
    <br/>
    <br/>
    
    <div style="width:280px; margin-left:auto; margin-right:auto;">
        <a href="${url('manage_users', limit = c.limit, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = c.order_by, order_dir=c.order_dir)}">First</a>
        <a href="${url('manage_users', page = c.page - 1 , limit = c.limit, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = c.order_by, order_dir=c.order_dir)}">Previous</a>
        <form method="POST" action="${url('manage_users')}" style="display:inline">
            Page <input class="number" name="page" type="text" value="${c.page}"/> of ${c.page_count} <input type="submit" value="Go"/>
            <input type="hidden" name="limit" value="${c.limit}"/>
            <input type="hidden" name="order_by" value="${c.order_by}"/>
            <input type="hidden" name="order_dir" value="${c.order_dir}"/>
            <input type="hidden" name="license_criteria" value="${c.license_criteria}"/>
            <input type="hidden" name="email_criteria" value="${c.email_criteria}"/>
        </form>
        <a href="${url('manage_users', limit = c.limit, page = c.page + 1 if c.page < c.page_count else c.page_count, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = c.order_by, order_dir=c.order_dir)}">Next</a>
        <a href="${url('manage_users', limit = c.limit, page = c.page_count, license_criteria = c.license_criteria, email_criteria = c.email_criteria, order_by = c.order_by, order_dir=c.order_dir)}">Last</a>
    </div>

    <br/>
    <div style="width:500px; margin-left:auto; margin-right:auto; border-top: 1px solid #999999">
    </div>
    <br/>
    <div>
        <form method="POST" action="${url('manage_users')}">
            <div>
                Showing <input class="number" name="limit" type="text" value="${c.limit}"/> users per page, matching email <input id="input_email_criteria" name="email_criteria" type="text" value="${c.email_criteria}"/>
                , and license
                <select name="license_criteria" style="width:90px; font-size:0.9em;">
                    <option value="all" ${c.select_license_criteria['all']}>Any</option>
                    <option value="except_none" ${c.select_license_criteria['except_none']}>Any but 'No License'</option>
                    <option value="none" ${c.select_license_criteria['none']}>No License</option>
                    <option value="confirm" ${c.select_license_criteria['confirm']}>Confirm</option>
                    <option value="freemium" ${c.select_license_criteria['freemium']}>Freemium</option>
                    <option value="bronze" ${c.select_license_criteria['bronze']}>Bronze</option>
                    <option value="silver" ${c.select_license_criteria['silver']}>Silver</option>
                    <option value="gold" ${c.select_license_criteria['gold']}>Gold</option>
                </select>
            </div>
            <input type="submit" id="display_submit" value="Show Matching Users"/>            
        </form>
    </div>
</fieldset>

<br/>
<br/>
<fieldset class="new-user">
    <legend style="margin-left:15px;">Add new users</legend>
    <div class= "users-list-header">
        <div class="email">User email</div>
        <div class="license">License</div>
        <div class="password">Password</div>
    </div>
    <div class="new-user-entry">
        <form method="POST" action="${url('manage_users_add')}">
            <input id="input_email" type="text" name="email"/>
            <select name="license">
                <option value="none" selected="selected">No License</option>
                <option value="freemium">Freemium</option>
                <option value="bronze">Bronze</option>
                <option value="silver">Silver</option>                                
                <option value="gold">Gold</option>                
            </select>
            <input id="input_pwd" type="password" name="password"/>
            <input type="hidden" name="limit" value="${c.limit}"/>
            <input type="submit" value="Add"/>
        </form>
        
    </div>
</fieldset>
