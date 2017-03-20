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