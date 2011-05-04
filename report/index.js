$(function(){
  $(".nodeinfo").hide();
  $("tr[rel^=node]").mouseover(function(event) {
    var $info = $("#"+$(this).attr('rel'));
    $info && $info.css({left: event.pageX, top: event.pageY+10, position:'absolute'}).show();
  }).mouseout(function(event) {
    var $info = $("#"+$(this).attr('rel'));
    $info && $info.hide();
  });
});
