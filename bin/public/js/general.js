var websocket;

function connect(onMessage)
{
	if ("WebSocket" in window)
	{
		if (websocket)
			websocket.close();
			
		websocket = new WebSocket("ws://" + ip + ":" + port);
		websocket.onmessage = onMessage;
	}
	else
	{
		websocket = null;
	}
}

function  setProgress(progressBar, progress){
	var valueFixed = parseFloat(progress).toFixed();

	if (valueFixed >= 100) {
		valueFixed = 100;
		progressBar.removeClass("active");
	}
	else if (valueFixed <= 100) {
		if (valueFixed < 0)
			valueFixed = 0;
		
		if (!progressBar.hasClass("active"))
			progressBar.addClass("active");
	}

	progressBar.attr("style", "width:" + valueFixed + "%");
	progressBar.html(valueFixed + " %");
}