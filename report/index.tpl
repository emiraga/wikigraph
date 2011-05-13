<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">

  <title>emiraga/wikigraph @ GitHub</title>

  <style type="text/css">
    body { margin-top: 1.0em; background-color: #c7dbfe;
      font-family: "Helvetica,Arial,FreeSans"; color: #000000; }
    #container { margin: 0 auto; width: 700px; }
    h1.title { font-size: 2.8em; color: #b87441; margin-bottom: 3px; }
    h1 .small { font-size: 0.4em; }
    h1 a { text-decoration: none; }
    h2 { font-size: 1.5em; /*color: #b87441;*/ }
    h3 { text-align: center; color: #b87441; }
    a { color: black; }
    a:hover { color: red !important; }
    .description { font-size: 1.2em; margin-bottom: 30px; margin-top: -1px; font-style: italic;}
    .download { float: right; }
    pre { background: #000; color: #fff; padding: 15px;}
    hr { border: 0; width: 100%; border-bottom: 1px solid #000}
    .footer { text-align:center; padding-top:30px; font-style: italic; }
    p { line-height: 25px; text-align: justify; }
    .nodeinfo { 
      border: 1px solid black;
      width: 440px; height 20px; background-color: white; padding:0px 5px 0px 5px;
      -o-box-shadow: 5px 5px 5px #000;
      -moz-box-shadow: 5px 5px 5px #000;
      -webkit-box-shadow: 5px 5px 5px #000;
      box-shadow: 5px 5px 5px #000;
      /* seriously wtf? */
      filter: progid:DXImageTransform.Microsoft.Blur(PixelRadius=3,MakeShadow=true,ShadowOpacity=0.30);
      -ms-filter: "progid:DXImageTransform.Microsoft.Blur(PixelRadius=3,MakeShadow=true,ShadowOpacity=0.30)";
      zoom: 1;
    }
    .nodeinfo p { font-size: 0.9em; line-height: 14px; margin-bottom: 3px; margin-top: 3px; }
    /*.nodeinfo .left { position:absolute; top:100px; left:0px; width: 220px; border: 0 solid black; }
    .nodeinfo .right { position:absolute; top:100px; left:220px; width: 220px; border: 0 solid black; }*/
    .nodeinfo .left { display: inline-block;  width: 215px; border-right: 1px solid black; }
    .nodeinfo .right { display: inline-block; width: 215px; }
    .nodeinfo h3 { line-height: 0px; }
    /*.nodeinfo img { margin-left: -10px; }*/
    tr:hover { background-color: #c7cbee; }
    .imglabel { text-align: center; font-size: 14px; }
    img.figure { border: 1px solid black; }
    .note { /*background-color: white;*/ border: 1px solid gray; }
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

    <h1 class="title"><a href="http://github.com/emiraga/wikigraph">wikigraph</a>
      <span class="small">by <a href="https://github.com/emiraga">emiraga</a></span></h1>

    <div class="description">
      English Wikipedia as graph.
    </div>

    <p>
      Data is generated using <a href="http://dumps.wikimedia.org/enwiki/20110405/">English wikipedia dumps (20110405)</a>. 
      You may remember <a href="http://www.netsoc.tcd.ie/~mu/wiki/">similar efforts from 2007</a>.
      Technical details and source code are available at <a href="http://github.com/emiraga/wikigraph">github page</a>.
    </p>

    <h2>Graph info</h2>

    <table border="0" width="400px">
    <tbody>
      <tr><th>Year<th>2007<th>2011</tr>
      <tr><th>Articles<td>2301486<td><%=art.num_articles%></tr>
      <tr><th>Article links (AL)<td>55550003<td><%=art.num_links%></tr>
      <tr><th>Categories<td><td><%=cat.num_categories%></tr>
      <tr><th>Category links (CL)<td><td><%=cat.num_links%></tr>
      <tr><th><abbr title="Articles+Categories">All nodes</abbr><td><td><%=art.num_nodes%></tr>
    </tbody>
    </table>

    <!--
    <p>
      Data from 2007 was reported by <a href="http://www.netsoc.tcd.ie/~mu/wiki/">Stephen Dolan</a>, I have no idea if it's correct.
      Ratio of links versus articles does seem to be increased drastically over last 4 years.
    </p>
    -->

    <p>
      For instance, article "Penny Can" has 3 outgoing <b>article links</b> (AL) and 2 <b>category links</b> (CL). Former are directed whereas latter are undirected.
      Each <b>article</b> or <b>category</b> is one <b>node</b> in a graph.
    </p>
    <img class="figure" src="http://i.imgur.com/iCJx9.png" />
    <div class="imglabel">Sample node with 3 AL and 2 CL.</div>

    <h2>Article PageRank</h2>

    <table width="100%" border="0">
      <tbody>
        <% var pr=art.pageranks; for ( var i = 0; i != pr.length; i++ ) {  %>
          <tr rel="node<%=pr[i][1]%>" >
            <td><a href="<%=enwiki+pr[i][2]%>"><%=pr[i][2]%></a></td>
            <td>(<%=(100*pr[i][0]).toFixed(3)%>%)</td>
          </tr>
        <% } %>
      </tbody>
    </table>
    <p>
      <a href="http://en.wikipedia.org/wiki/PageRank">PageRank</a> is probability that surfer of will be located at a particular article, following ALs randomly.
      For technical reasons, links from one article to another are counted just once (no duplicates).
    </p>

    <h2>Strongly connected components</h2>

    <table border="0" width="300px">
      <tbody>
        <tr><th>Component size</th><th>How many?</th></tr>
        <% for ( var i = 0; i != art.scc.length; i++ ) { %>
          <tr>
            <td> <%=art.scc[i][0]%> articles</td>
            <td> <%=art.scc[i][1]%></td>
          </tr>
        <% } %>
      </tbody>
    </table>
    
    <p>
      Compared to reports from <a href="http://www.netsoc.tcd.ie/~mu/wiki/">2007</a>, now we see more a lot more of non-trivial small components.
    </p>

    <h2>Minimum distance (AL)</h2>
 
    <% if(art.nodes_done != art.num_nodes) { %>
      This data is based on random sampling of <%=art.nodes_done%> (<%=(100*art.nodes_done/art.num_nodes).toFixed(1)%>%) nodes.
    <% } %>
    <table border="0" width="400px">
      <tbody>
        <tr><th>Average reachable articles<td><%= (art.reachable_sum / art.nodes_done ).toFixed(2) %></th></tr>
        <tr><th>Average distance (clicks)<td><%= (art.distance_sum / art.reachable_sum ).toFixed(2) %></th></tr>
      </tbody>
    </table>

    <!--<p>Distance between articles (AL):</p>-->

    <img class="figure" src="http://chart.apis.google.com/chart?chco=76A4FB&chds=0,<%=Math.max.apply(null,art.dist_spectrum)+1%>&chxt=x&chbh=a&chs=696x140&cht=bvs&chd=t:<%=art.dist_spectrum.join(",")%>&chm=D,4D89F9,0,0,2,1" />
    <div class="imglabel">Distance spectrum (AL).</div>


    <h2>Closeness centrality (AL)</h2>

    <table width="100%" border="0">
      <tbody>
        <tr>
          <th>Nodes<th>Average distace
        </tr>
        <% for ( var i = 0; i != art.close_centr.length; i++ ) { var info = art.close_centr[i]; %>
          <tr rel="node<%=info.node%>" >
            <td><a href="<%=enwiki+art.close_centr[i].name%>"><%=art.close_centr[i].name%></a><td> (<%=art.close_centr[i].avg_dist.toFixed(3)%>)
          </tr>
        <% } %>
      </tbody>
    </table>

    <hr />

    <h1>Category links</h1>

    <p>
      Category links (CL) are essentially different from AL. Link is formed when one node (article/category) is declared to be in a category (see example at the top of the page). CL are undirected, i.e. node belongs to another category and category has nodes in it.
    </p>
    <p>
      An example of CL path:
      <a href="http://en.wikipedia.org/wiki/Prague">Prague</a> &#8658;
      <a href="http://en.wikipedia.org/wiki/Category:Cities_and_towns_in_the_Czech_Republic">Category:Cities_and_towns_in_the_Czech_Republic</a> &#8658;
      <a href="http://en.wikipedia.org/wiki/Category:Karlovy_Vary">Category:Karlovy_Vary</a> &#8658;
      <a href="http://en.wikipedia.org/wiki/Grandhotel_Pupp">Grandhotel_Pupp</a>
    </p>

    <h2>Minimum distance (CL)</h2>
 
    <% if(cat.nodes_done != cat.num_nodes) { %>
      This data is based on random sampling of <%=cat.nodes_done%> (<%=(100*cat.nodes_done/cat.num_nodes).toFixed(1)%>%) nodes.
    <% } %>
    <table border="0" width="400px">
      <tbody>
        <tr><th>Average reachable nodes<td><%= (cat.reachable_sum / cat.nodes_done ).toFixed(2) %></th></tr>
        <tr><th>Average distance<td><%= (cat.distance_sum / cat.reachable_sum ).toFixed(2) %></th></tr>
      </tbody>
    </table>

    <!-- <p>Distance between nodes (CL):</p> -->

    <img class="figure" src="http://chart.apis.google.com/chart?chco=76A4FB&chds=0,<%=Math.max.apply(null,cat.dist_spectrum)+1%>&chxt=x&chbh=a&chs=696x140&cht=bvs&chd=t:<%=cat.dist_spectrum.join(",")%>&chm=D,4D89F9,0,0,2,1" />
    <div class="imglabel">Distance spectrum (CL).</div>

    <h2>Closeness centrality (CL)</h2>
    <table width="100%" border="0">
      <tbody>
        <tr>
          <th>Nodes<th>Average distace
        </tr>
        <% for ( var i = 0; i != cat.close_centr.length; i++ ) { var info = cat.close_centr[i]; %>
          <tr rel="node<%=info.node%>" >
            <td><a href="<%=enwiki+cat.close_centr[i].name%>"><%=cat.close_centr[i].name%></a><td> (<%=cat.close_centr[i].avg_dist.toFixed(3)%>)
          </tr>
        <% } %>
      </tbody>
    </table>

    <!--
    <h2>Interactive</h2>

    <p>
      I don&#39;t have public server to host a web service. However, there are 
      <a href="http://en.wikipedia.org/wiki/Wikipedia:Six_degrees_of_Wikipedia#External_links">other tools</a> available for computing minimum distance.
    </p>
    -->
    <hr />

    <h1>Conclusion</h1>

    <p>
    Even though, average number of article links per one article has increased from 25 to 50, average distance today is larger compared to 2007.
    This might indicate that additional links contribute to create highly connected clusters of nodes. Within one group of articles distances are
    quite small (visible in the peak of &quot;distance spectrum&quot;), but distance to other groups is larger. Running 
    <abbr title="Strongly connected components">SCC</abbr> algorithm did reveal only a few of such components, whereas majority are still in one giant
    bundle of nodes. Resolving this issue would mean to cluster nodes into significantly smaller groups.
    This could be a topic of further investigation (suggestions are welcome).
    </p>

    <p>
    Graph with CL is a new concept that was not done before.
    </p>

    <p>
      You can download source of this project in either
      <a href="http://github.com/emiraga/wikigraph/zipball/master">zip</a> or
      <a href="http://github.com/emiraga/wikigraph/tarball/master">tar</a> formats.
      You can also clone the project with <a href="http://git-scm.com">Git</a>
      by running:
      <pre>$ git clone git://github.com/emiraga/wikigraph</pre>
    </p>


    <div class="footer">
      get the source code on GitHub : <a href="http://github.com/emiraga/wikigraph">emiraga/wikigraph</a>
    </div>

  </div>

  <div id="interesing_nodes">
    <% for ( var i = 0; i != interesting_nodes.length; i++ ) { var info = interesting_nodes[i]; %>
      <div id="node<%=info.node%>" class="nodeinfo">
        <h3><%=info.name%></h3>
        <div class="left">
          <p><b>Articles</b> at a particular distance:</p>
          <img src="http://chart.apis.google.com/chart?chco=76A4FB&chds=0,<%=info.art.max_dist%>&chxt=x,y&chxr=1,0,<%=info.art.max_dist%>&chbh=a&chs=215x100&cht=bvs&chd=t:<%=info.art.count_dist.join(",")%>" />
          <p>Incoming article links: <%=info.art.in_degree%></p>
          <p>Outgoing article links: <%=info.art.out_degree%></p>
          <p>Rachable articles: <%=info.art.stat.reachable%></p>
          <p>Average distance: <%=(info.art.stat.closeness).toFixed(4)%></p>
        </div>
        <div class="right">
          <p><b>Category links</b></p>
          <p>Nodes at a particular distance:</p>
          <img src="http://chart.apis.google.com/chart?chco=76A4FB&chds=0,<%=info.cat.max_dist%>&chxt=x,y&chxr=1,0,<%=info.cat.max_dist%>&chbh=a&chs=215x100&cht=bvs&chd=t:<%=info.cat.count_dist.join(",")%>" />
          <p>Category links: <%=info.cat.in_degree%></p>
          <p>Rachable nodes: <%=info.cat.stat.reachable%></p>
          <p>Average distance: <%=(info.cat.stat.closeness).toFixed(4)%></p>
        </div>
      </div>
    <% } %>
  </div>
  <script src="index.js"></script>
</body>
</html>

