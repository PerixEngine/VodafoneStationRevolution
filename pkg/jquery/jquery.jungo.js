/* Add a tooltip to an object with a caption and optional image */
jQuery.fn.addTooltip = function(caption, img) {
    var xOffset = 0;
    var yOffset = 20;

    if (!document.env_enable_tooltips)
	return this;

    this.hover(function(e) {
	var div, caption_div;
        $("body").append(div = $("<div/>").addClass("tooltip").hide());
	if (img)
	    div.append($("<img/>").attr("src", img).attr("alt", caption));
	div.append((caption_div = $("<div/>")).html(caption));
	$("body").append($("<iframe/>").addClass("tooltip").attr("src",
	    "javascript: false").attr("style", "filter: alpha(opacity = 0); " +
	    "background:none; width: " + div.outerWidth() + "px;" +
	    "height: " + div.outerHeight() + "px; opacity: 0;"));

	$(".tooltip").css("top",(e.pageY + xOffset) + "px").
	    css("left",(e.pageX + yOffset) + "px");
	
	$("div.tooltip").fadeIn("fast");
	if (img)
	    caption_div.trim_str($("img", div).width());
	
    }, function(){
	$(".tooltip").remove();
    });

    this.mousemove(function(e){
        $(".tooltip").css("top",(e.pageY + xOffset) + "px").
	    css("left",(e.pageX + yOffset) + "px");
    });
    
    return this;
};

/* If the text in "this" is longer than "width", trim it and add an ellipsis.
 * If "width" is not given, use parent's width(). */
jQuery.fn.extend({
    trim_str: function(width, trim_head)
    {
	if (width === undefined)
	    width = this.parent().width();

        this.width("auto").css('white-space','nowrap');
        if (this.width() > width)
        {
            var s = this.text();
            var i = s.length;
            while (--i && this.width() > width)
	    {
                this.text(trim_head ?
		    "..." + s.substr(s.length - i) : s.substr(0, i) + "...");
	    }
	    this.attr("title", s);
        }
	this.width(width);
        return this;
    }
});

function scrollableReload()
{
    /* Members of scrollable that we are using. If the API of scrollable 
     * changes, we should change them as well */
    var conf = this.getConf();
    var items = this.getItems();
    var index = this.getIndex();
    var reload = this.reload;
    var seekTo = this.seekTo;
    /* Current offset of scrollable */
    var cur_left = parseInt(this.getItemWrap().css('left'));

    (function ()
    {
	var item = items.filter("." + conf.activeClass);
	var item_index = item.prevAll().length;
	var del_list;
	var shift = 0;

	/* NOTE: this function can not work with vertical scrollable. */
	if (items.length == 0 || conf.vertical)
	    return;

	del_list = items.filter("." + conf.deleteClass);
	if (del_list.length == 0)
	{
	    reload();
	    return;
	}

	/* Number of preceding siblings of the active item that will be
	 * removed. The index of active item is shifted by this number. */
	shift = items.filter(":lt(" + item_index + ")").
	    filter("." + conf.deleteClass).length;
	if (item.hasClass(conf.deleteClass))
	{
	    var window_tail = Math.round(conf.size/2);
	    /* Number of following siblings of the active item that are not
	     * going to be removed. */
	    var tail_size = items.filter(":gt(" + item_index + ")").
		filter(":not(."+conf.deleteClass+")").length;

	    /* This will be true iff there is no following sibling of the
	     * active item not going to be removed. */
	    if (tail_size < window_tail)
		shift += window_tail - tail_size;

	    /* Set new active item now to prevent synchronization problems. */
	    items.removeClass(conf.activeClass).
		filter(":not(."+conf.deleteClass+")").
		eq(item_index - shift).
		addClass(conf.activeClass);
	}

	del_list.filter(':last').data("last-deleted", true);
	del_list.animate({ 'width': '0px' },
	    { 'duration': conf.speed, 'easing': conf.easing,
	    'queue': false, 'complete': function() {
		if (!$(this).data("last-deleted"))
		    return;

		/* If this is the last item in del_list, then remove the list and
		 * reload the scrollable. */
		del_list.remove();
		reload();
	    }
	});
	seekTo(index - shift, conf.speed);
    })();
    return this;
};
