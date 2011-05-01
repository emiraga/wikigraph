<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">

  <title>emiraga/wikigraph @ GitHub</title>

  <style type="text/css">
    body {
      margin-top: 1.0em;
      background-color: #c7dbfe;
      font-family: "Helvetica,Arial,FreeSans";
      color: #000000;
    }
    #container {
      margin: 0 auto;
      width: 700px;
    }
    h1 { font-size: 2.8em; color: #b87441; margin-bottom: 3px; }
    h1 .small { font-size: 0.4em; }
    h1 a { text-decoration: none; }
    h2 { font-size: 1.5em; color: #b87441; }
    h3 { text-align: center; color: #b87441; }
    a { color: black; }
    .description { font-size: 1.2em; margin-bottom: 30px; margin-top: -1px; font-style: italic;}
    .download { float: right; }
    pre { background: #000; color: #fff; padding: 15px;}
    hr { border: 0; width: 80%; border-bottom: 1px solid #aaa}
    .footer { text-align:center; padding-top:30px; font-style: italic; }
    p { line-height: 25px; }
    .nodeinfo { border: 1px solid black; width: 220px; height 220px; background-color: white; padding:0px 5px 0px 5px;
      -o-box-shadow: 5px 5px 5px #000;
      -moz-box-shadow: 5px 5px 5px #000;
      -webkit-box-shadow: 5px 5px 5px #000;
      box-shadow: 5px 5px 5px #000;
      /* seriously wtf? */
      filter: progid:DXImageTransform.Microsoft.Blur(PixelRadius=3,MakeShadow=true,ShadowOpacity=0.30);
      -ms-filter: "progid:DXImageTransform.Microsoft.Blur(PixelRadius=3,MakeShadow=true,ShadowOpacity=0.30)";
      zoom: 1;
    }
    .nodeinfo p {
      font-size: 0.9em;
      line-height: 14px;
      margin-bottom: 3px;
      margin-top: 3px;
    }
    tr:hover {
      background-color: #c7cbee;
    }
  </style>
  <script src="http://ajax.googleapis.com/ajax/libs/jquery/1.5.1/jquery.min.js"></script>
</head>

<body>
  <a href="http://github.com/emiraga/wikigraph"><img style="position: absolute; top: 0; right: 0; border: 0;" 
  src="http://s3.amazonaws.com/github/ribbons/forkme_right_darkblue_121621.png" alt="Fork me on GitHub" /></a>

  <div id="container">

    <!--
    <div class="download">
      <a href="http://github.com/emiraga/wikigraph/zipball/master">
        <img border="0" width="90" src="http://github.com/images/modules/download/zip.png"></a>
      <a href="http://github.com/emiraga/wikigraph/tarball/master">
        <img border="0" width="90" src="http://github.com/images/modules/download/tar.png"></a>
    </div>
    -->

    <h1><a href="http://github.com/emiraga/wikigraph">wikigraph</a>
      <span class="small">by <a href="http://github.com/emiraga">emiraga</a></span></h1>

    <div class="description">
      English Wikipedia as graph.
    </div>

    <p>
      Data is generated using <a href="http://dumps.wikimedia.org/enwiki/20110317/">English wikipedia dumps</a>. 
      You may remember <a href="http://www.netsoc.tcd.ie/~mu/wiki/">similar efforts from 2007</a>.
      Technical details and source code are available at <a href="http://github.com/emiraga/wikigraph">github page</a>.
    </p>

    <h2>Graph info</h2>

    <table border="0" width="400px">
    <tbody>
      <tr><th>Year<th>2007<th>2011</tr>
      <tr><th>Articles<td>2301486<td><%=num_articles%></tr>
      <tr><th>Article links<td>55550003<td><%=num_artlinks%></tr>
      <tr><th>Categories<td><td><%=num_categories%></tr>
      <tr><th>Category links<td><td><%=num_catlinks%></tr>
    </tbody>
    </table>

    <p>
      Data from 2007 was reported by <a href="http://www.netsoc.tcd.ie/~mu/wiki/">Stephen Dolan</a>, I have no idea if it's correct.
      Ratio of links versus articles does seem to be increased drastically over 4 years.
    </p>

    <h2>Article PageRank</h2>

    <!-- Ohmigod we are using tables!?! -->
    <table width="400px" border="0">
      <tbody>
        <% for ( var i = 0; i != pageranks.length; i++ ) { %>
          <tr>
            <td><a rel="node<%=pageranks[i][1]%>" href="<%=enwiki+pageranks[i][2]%>"><%=pageranks[i][2]%></a></td>
            <td>(<%=(100*pageranks[i][0]).toFixed(3)%>%)</td>
          </tr>
        <% } %>
      </tbody>
    </table>
    <p>
      <a href="http://en.wikipedia.org/wiki/PageRank">PageRank</a> is probability that random surfer of will be located at a particular page.
      For technical reasons, links from one article to another are counted just once (no duplicates).
    </p>

    <h2>Strongly connected components (Articles)</h2>

    <table border="0" width="300px">
      <tbody>
        <tr><th>Component size</th><th>How many?</th></tr>
        <% for ( var i = 0; i != scc.length; i++ ) { %>
          <tr>
            <td> <%=scc[i][0]%> articles</td>
            <td> <%=scc[i][1]%></td>
          </tr>
        <% } %>
      </tbody>
    </table>
    
    <p>Compared to reports from <a href="http://www.netsoc.tcd.ie/~mu/wiki/">2007</a>, now we see more non-trivial small components.</p>

    <h2>Minimum distance between articles</h2>
 
    This data is based on random sampling of <%=articles_done%> (<%=(100*articles_done/num_articles).toFixed(1)%>%) articles.

    <table border="0" width="400px">
      <tbody>
        <tr><th>Average reachable articles<td><%= (reachable_sum / articles_done ).toFixed(2) %></th></tr>
        <tr><th>Average distance<td><%= (distance_sum / reachable_sum ).toFixed(2) %></th></tr>
      </tbody>
    </table>

    <h2>Interactive</h2>

    <p>
      I don&#39;t have public server to host a web service. However, there are 
      <a href="http://en.wikipedia.org/wiki/Wikipedia:Six_degrees_of_Wikipedia#External_links">other tools</a> available.
    </p>

    <h2>Download</h2>

    <p>
      You can download this project in either
      <a href="http://github.com/emiraga/wikigraph/zipball/master">zip</a> or
      <a href="http://github.com/emiraga/wikigraph/tarball/master">tar</a> formats.
    </p>
    <p>You can also clone the project with <a href="http://git-scm.com">Git</a>
      by running:
      <pre>$ git clone git://github.com/emiraga/wikigraph</pre>
    </p>

    <!--
    <h2>Closeness centrality</h2>
    <ol>
    <% for ( var i = 0; i != close_centr.length; i++ ) { %>
      <li><a href="<%=enwiki+close_centr[i].name%>"><%=close_centr[i].name%> (<%=close_centr[i].avg_dist.toFixed(3)%>)</a></li>
    <% } %>
    </ol>
    -->

    <div class="footer">
      get the source code on GitHub : <a href="http://github.com/emiraga/wikigraph">emiraga/wikigraph</a>
    </div>

  </div>

  <% for ( var i = 0; i != interesting_nodes.length; i++ ) { var info = interesting_nodes[i]; %>
    <div id="node<%=info.node%>" class="nodeinfo">
    <% if(info.is_article) { %>
      <h3><%=info.name%></h3>
      <p>Articles at a particular distance:</p>
      <img src="http://chart.apis.google.com/chart?chds=0,<%=info.max_dist%>&chxt=x,y&chxr=1,0,<%=info.max_dist%>&chbh=a&chs=220x100&cht=bvs&chd=t:<%=info.count_dist.join(",")%>" />
      <p>Incoming article links: <%=info.in_degree%></p>
      <p>Outgoing article links: <%=info.out_degree%></p>
      <p>Rachable articles: <%=info.reachable%></p>
      <p>Average distance: <%=(info.distance / info.reachable).toFixed(4)%></p>
    <% } %>
    </div>
  <% } %>
  <script src="index.js"></script>
</body>
</html>

