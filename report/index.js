$(function(){
  $(".nodeinfo").hide();
  $("a[rel^=node]").mouseover(function(event) {
    $this = $(this);
    $info = $("#"+$this.attr('rel'));
    $info && $info.css({left: event.pageX, top: event.pageY+10, position:'absolute'}).show();
  }).mouseout(function(event) {
    $this = $(this);
    $info = $("#"+$this.attr('rel'));
    $info && $info.hide();
  });
});

