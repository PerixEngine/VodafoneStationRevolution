/*
# jQuery Truncate Text Plugin #

Simple plugin that truncates a text either at its end or middle based on a given width or it's elements width. The width calculation takes into account all the original elements css styles like font-size, font-family, font-weight, text-transform, letter-spacing etc.  
Additionally if the text has been shortened you can set a class to be appended to the element and/or set the "title" attribute to the original text.

## Usage ##


    $('.class').truncate();

    $('.class').truncate({
    	width: 'auto',
    	token: '&hellip;',
    	center: false,
    });

## Options ##

- **width** (int) Width to which the text will be shortened *[default: auto]*
- **token** (string) Replacement string for the stripped part *[default: '&amp;hellip;']*
- **center** (bool) Shortens at the center of the string if set to 'true' *[default: false]*
- **addclass** (string) Add a class to the truncated strings element *[default: false]*
- **addtitle** (bool) Add/Set "title" attribute with original text to the truncated strings element *[default: false]*

## License ##

Released under the MIT license.

*/


(function ($) {
	function isNumber(n) {
		return !isNaN(parseFloat(n)) && isFinite(n);
	}
	function findTruncPoint(maxWidth, text, start, end, $workerEl, token, fromEnd) {
		var opt1,
			opt2,
			mid;

		if (fromEnd) {
			opt1 = start === 0 ? '' : text.slice(-start);
			opt2 = text.slice(-end);
		} else {
			opt1 = text.slice(0, start);
			opt2 = text.slice(0, end);
		}

		if ($workerEl.html(opt2 + token).width() < $workerEl.html(opt1 + token).width()) {
			return end;
		}

		mid = parseInt((start + end) / 2, 10);
		opt1 = fromEnd ? text.slice(-mid) : text.slice(0, mid);

		$workerEl.html(opt1 + token);
		if ($workerEl.width() === maxWidth) {
			return mid;
		}

		if ($workerEl.width() > maxWidth) {
			end = mid - 1;
		} else {
			start = mid + 1;
		}

		return findTruncPoint(maxWidth, text, start, end, $workerEl, token, fromEnd);
	}

	$.fn.truncate = function (options) {
		var defaults = {
			width: 'auto',
			token: '&hellip;',
			center: false,
			addclass: false,
			addtitle: false
		};
		options = $.extend(defaults, options);

		return this.each(function () {
			var $element = $(this),
				fontCSS = {
					'fontFamily': $element.css('fontFamily'),
					'fontSize': $element.css('fontSize'),
					'fontStyle': $element.css('fontStyle'),
					'fontWeight': $element.css('fontWeight'),
					'font-variant': $element.css('font-variant'),
					'text-indent': $element.css('text-indent'),
					'text-transform': $element.css('text-transform'),
					'letter-spacing': $element.css('letter-spacing'),
					'word-spacing': $element.css('word-spacing'),
					'display': 'none'
				},
				elementText = $element.text(),
				$truncateWorker = $('<span/>').css(fontCSS).html(elementText).appendTo('body'),
				originalWidth = $truncateWorker.width(),
				truncateWidth = isNumber(options.width) ? options.width : $element.innerWidth() - 5, // added support of padding and extra space to prevent of text wrap in some browsers
				truncatedText;

			if (originalWidth > truncateWidth) {
				$truncateWorker.text('');
				if (options.center) {
					truncateWidth = parseInt(truncateWidth / 2, 10) + 1;
					truncatedText = elementText.slice(0, findTruncPoint(truncateWidth, elementText, 0, elementText.length, $truncateWorker, options.token, false))
									+ options.token
									+ elementText.slice(-1 * findTruncPoint(truncateWidth, elementText, 0, elementText.length, $truncateWorker, '', true));
				} else {
					truncatedText = elementText.slice(0, findTruncPoint(truncateWidth, elementText, 0, elementText.length, $truncateWorker, options.token, false)) + options.token;
				}

				if (options.addclass) {
					$element.addClass(options.addclass);
				}

				if (options.addtitle) {
					$element.attr('title', elementText);
				}

				$element.empty().append(truncatedText);

			}

			$truncateWorker.remove();
		});
	};
})(jQuery);
