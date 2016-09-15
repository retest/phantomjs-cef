var page = require('webpage').create();
page.viewportSize = { width: 1920, height: 1200 };
page.clipRect = { top: 410, left: 450, width: 520, height: 545 };
page.settings.userAgent = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/45.0.2454.101 Chrome/45.0.2454.101 Safari/537.36';

//page.onPaint = function(dirtyRects, width, height, isPopup) {
//  console.log("Paint event of width = " + width + ", height = " + height + "! Was a popup? " + isPopup + ", Dirty rects = " + JSON.stringify(dirtyRects, 1));
//};


page.open('http://www.wetteronline.de/regenradar/nordrhein-westfalen')
.then(()=>{
	return phantom.wait(5000);
})
.then(()=>{
	renderAndclickForward(1);
})

function renderAndclickForward(i){
	console.log(i);
	if (i===8){
		phantom.exit();
	} else {
		//page.render('nrw'+i+'.png')
		//.then(()=>{
		//	return page.sendMouseEvent('click', 'div#nextButton');
		//})
		page.sendMouseEvent('click', 'div#nextButton')
		.then(()=>{
			return page.sendMouseEvent('click', 'html');
		})
		.then(()=>{
			return phantom.wait(3000);
		})
		.then(()=>{
			var j=i+1;
			renderAndclickForward(j);
		})
	}
}
