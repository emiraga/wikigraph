// Lightweight inline SVG bar chart (replaces the now-defunct Google Image
// Charts API). No external service, library or account required.
//
// @param values  array of numbers (bar heights)
// @param opts    {width, height, max, barColor, lineColor, showX, showY, line, cls}
function svgBarChart(values, opts) {
  opts = opts || {};
  var width = opts.width || 696;
  var height = opts.height || 140;
  var barColor = opts.barColor || '#76A4FB';
  var lineColor = opts.lineColor || '#4D89F9';
  var showX = opts.showX !== false;   // x-axis labels on by default
  var showY = !!opts.showY;
  var showLine = !!opts.line;
  var n = values.length;

  var max = opts.max;
  if (max === undefined) {
    max = 0;
    for (var k = 0; k < n; k++) {
      if (values[k] > max) max = values[k];
    }
  }
  if (max <= 0) max = 1;

  var padL = showY ? 46 : 6;
  var padR = 6;
  var padT = 8;
  var padB = showX ? 16 : 6;
  var plotW = width - padL - padR;
  var plotH = height - padT - padB;
  var slot = plotW / Math.max(n, 1);
  var barW = slot * 0.8;
  var baseY = padT + plotH;

  var p = [];
  p.push('<svg xmlns="http://www.w3.org/2000/svg" width="' + width + '" height="' + height +
    '" viewBox="0 0 ' + width + ' ' + height +
    '" font-family="Helvetica,Arial,sans-serif" font-size="10"' +
    (opts.cls ? ' class="' + opts.cls + '"' : '') + '>');

  // axes
  p.push('<line x1="' + padL + '" y1="' + baseY + '" x2="' + (padL + plotW) +
    '" y2="' + baseY + '" stroke="#888"/>');
  if (showY) {
    p.push('<line x1="' + padL + '" y1="' + padT + '" x2="' + padL +
      '" y2="' + baseY + '" stroke="#888"/>');
    p.push('<text x="' + (padL - 4) + '" y="' + baseY + '" text-anchor="end" fill="#555">0</text>');
    p.push('<text x="' + (padL - 4) + '" y="' + (padT + 8) + '" text-anchor="end" fill="#555">' + max + '</text>');
  }

  var pts = [];
  for (var i = 0; i < n; i++) {
    var v = values[i] || 0;
    var barH = v / max * plotH;
    var x = padL + i * slot + (slot - barW) / 2;
    var y = baseY - barH;
    p.push('<rect x="' + x.toFixed(1) + '" y="' + y.toFixed(1) +
      '" width="' + barW.toFixed(1) + '" height="' + Math.max(barH, 0).toFixed(1) +
      '" fill="' + barColor + '"/>');
    pts.push((x + barW / 2).toFixed(1) + ',' + y.toFixed(1));
    if (showX) {
      p.push('<text x="' + (padL + i * slot + slot / 2).toFixed(1) + '" y="' + (baseY + 12) +
        '" text-anchor="middle" fill="#555">' + i + '</text>');
    }
  }

  if (showLine && pts.length > 1) {
    p.push('<polyline points="' + pts.join(' ') + '" fill="none" stroke="' +
      lineColor + '" stroke-width="1.5"/>');
  }

  p.push('</svg>');
  return p.join('');
}

function escapeHtml(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// Build the inner markup of a per-node tooltip from its inlined data.
function tooltipHtml(n) {
  return '<h3>' + escapeHtml(n.name) + '</h3>' +
    '<div class="left">' +
      '<p><b>Articles</b> at a particular distance:</p>' +
      svgBarChart(n.art.cd, {width: 215, height: 100, showY: true, max: n.art.md}) +
      '<p>Incoming article links: ' + n.art.in_degree + '</p>' +
      '<p>Outgoing article links: ' + n.art.out_degree + '</p>' +
      '<p>Rachable articles: ' + n.art.reachable + '</p>' +
      '<p>Average distance: ' + n.art.closeness.toFixed(4) + '</p>' +
    '</div>' +
    '<div class="right">' +
      '<p><b>Category links</b></p>' +
      '<p>Nodes at a particular distance:</p>' +
      svgBarChart(n.cat.cd, {width: 215, height: 100, showY: true, max: n.cat.md}) +
      '<p>Category links: ' + n.cat.in_degree + '</p>' +
      '<p>Rachable nodes: ' + n.cat.reachable + '</p>' +
      '<p>Average distance: ' + n.cat.closeness.toFixed(4) + '</p>' +
    '</div>';
}

$(function() {
  // Two main distance-spectrum charts.
  $('#al-spectrum-chart').html(
    svgBarChart(WIKIGRAPH_AL_SPECTRUM, {width: 696, height: 140, line: true, cls: 'figure'}));
  $('#cl-spectrum-chart').html(
    svgBarChart(WIKIGRAPH_CL_SPECTRUM, {width: 696, height: 140, line: true, cls: 'figure'}));

  // Index node data by id; tooltips are built lazily on first hover.
  var nodeMap = {};
  for (var i = 0; i < WIKIGRAPH_NODES.length; i++) {
    nodeMap[WIKIGRAPH_NODES[i].node] = WIKIGRAPH_NODES[i];
  }
  var $nodes = $('#interesing_nodes');

  var ensureTooltip = function(rel) {  // rel is e.g. "node12345"
    if (document.getElementById(rel)) return;  // already built
    var n = nodeMap[rel.substring(4)];
    if (!n) return;  // some listed rows have no per-node data
    $nodes.append('<div id="' + rel + '" class="nodeinfo">' + tooltipHtml(n) + '</div>');
  };

  $("tr[rel^=node]").mouseover(function(event) {
    var rel = $(this).attr('rel');
    ensureTooltip(rel);
    var $info = $("#" + rel);
    $info && $info.css({left: event.pageX, top: event.pageY + 10, position: 'absolute'}).show();
  }).mouseout(function(event) {
    var $info = $("#" + $(this).attr('rel'));
    $info && $info.hide();
  });
});
