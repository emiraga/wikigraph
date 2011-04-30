<h1>Title</h1>
<ol>
<% for ( var i = 0; i < users.length; i++ ) { %>
  <li><a href="<%=users[i].url%>"><%=users[i].name%></a></li>
<% } %>
</ol>
