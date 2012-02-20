## Template parameters
<%
    GT = c.GT
%>

<%inherit file="/layout.mako" />

<%def name="add_css()">
<link rel="stylesheet" href="/css/kws_mgt_specific.css" type="text/css" media="screen" />
</%def>

<h1>Teambox management</h1>

<%include file="/common/_glob_messages.mako"/>

<form action="${c.action_url|n}" method="post"> 

<input type="hidden" name="kws_mgt_specific_kws" value="${c.kws_id}"/>

<table>
<tr>
  <td class="toptableleftcol">Teambox: ${c.kws_name}</td>
  <td class="toptablerightcol">Creator: ${c.kws_creator}</td>
</tr>
<tr>
  <td class="toptableleftcol"><a target="_blank" href="${c.kws_ro_link}">Access this Teambox</a></td>
  <td class="toptablerightcol">Creation date: ${c.kws_date}</td>
</tr>
</table>

<table id="memberstattable"><tr>
  <td class="memberstattablecol">
  <fieldset id="memberfs" class="fs">
    <legend>Members</legend>
    <table>
${c.member_list|n}
    </table>
  </fieldset>
  </td>
  
  <td id="memberstatlastcol" class="memberstattablecol">
  <fieldset id="statfs" class="fs">
    <legend>Teambox statistics</legend>
    <table>
      <tr><td class="statfsleftcol">KFS size:</td><td class="statfsrightcol">${c.kws_file_size} MiB</td><td/></tr>
      <tr id="statfslastcol">
          <td class="statfsleftcol">KFS quota:</td>
          <td class="statfsrightcol"><input type="text" name="kws_quota" size="6" value="${c.kws_quota}"/> MiB</td>
          <td><input type="submit" name="kws_mgt_set_kws_quota" value="Change"/></td></tr>
    </table>
  </fieldset>
  </td>
</tr></table>

<br>  
<p><div id="backdiv"><a href="${c.back_url|n}">Back to previous page</a></div></p>

</form>

