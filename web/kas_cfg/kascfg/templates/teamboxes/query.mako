## Template parameters
<%
    GT = c.GT
%>

<%inherit file="/layout.mako" />

<%def name="add_css()">
<link rel="stylesheet" href="/css/kws_mgt_query.css" type="text/css" media="screen" />
</%def>

<script type="text/javascript">
<!--
function select_all_kws(val)
{
  var table = document.getElementById('kwstable');
  var cb_array = table.getElementsByTagName('input');
  for (var i = 0; i < cb_array.length; i++) cb_array[i].checked = val;
-->
}
</script>

<h1>Teambox management</h1>

<%include file="/common/_glob_messages.mako"/>

<form action="${c.action_url|n}" method="post"> 
<table id="kwstable">
  <thead>
    <tr>
      <th></th>
      <th></th>
      <th></th>
      <th class="kwstableth"><b>Users</b></th>
      <th class="kwstableth"><b>Size</b></th>
      <th class="kwstableth"><b>Creation date</b></th>
      <th class="kwstableth"><b>Organization</b></th>
    </tr>
  </thead>
  <tbody>
${c.kws_table_body|n}
  </tbody>
</table>

<div id="selectkwsdiv">
  <a href="#" onclick="select_all_kws(true);">Select All</a>
  /
  <a href="#" onclick="select_all_kws(false);">Deselect All</a>
</div>

<div id="actiondiv">
  <select name="kws_mgt_query_action_select">
    <option selected="selected" value="delete">Delete</option>
  </select>
  <input name="kws_mgt_query_action" type="submit" value="Apply"/>
</div>

</form>

<form action="${c.action_url|n}" method="post"> 
<table id="browsetable"><tr><td>
  <input id="browseshow" type="submit" name="kws_mgt_show_kws" value="Show:"/>
  <input id="browsetextperpage" type="text" name="kws_per_page" value="${c.kws_mgt_query_limit}" size="5"/>
  <span id="browsetextkws">Teamboxes</span>
  <span id="browsetextstart">Starting from record #</span>
  <input type="text" name="kws_start" value="${c.kws_mgt_query_offset}" size="5"/>
  <input id="browsenext" type="submit" name="kws_mgt_next_kws" value=">"/>
  <input id="browselast" type="submit" name="kws_mgt_last_kws" value=">>"/>
</td></tr></table>
</form>


