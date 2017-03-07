class Block
{
	constructor()
	{
		this.panel = $("<div class='panel panel-primary'></div>");
		this.head = $("<div class='panel-heading'></div>");
		this.body = $("<div class='panel-body'></div>");
		this.data = $("<ul id='blkdata' class='list-group'></ul>");
				
		this.panel.append(this.head);
		this.panel.append(this.body);
		this.panel.append(this.data);
		
		this.bestDeadline = null;
	}
	
	newBlock(block)
	{
		this.head.html("Block " + block["block"]);
		this.body.html("<div class='row'><div class='col-md-2 col-xs-4'>Time</div><div class='col-md-10 col-xs-8'>" + block["time"] + "</div></div>");
		this.body.append($("<div class='row'><div class='col-md-2 col-xs-4'>Scoop</div><div class='col-md-10 col-xs-8'>" + block["scoop"] + "</div></div>"));
		this.body.append($("<div class='row'><div class='col-md-2 col-xs-4'>Base target</div><div class='col-md-10 col-xs-8'>" + block["baseTarget"] + "</div></div>"));
		//this.body.append($("<div class='row'><div class='col-md-2 col-xs-4'>Gensignature</div><div class='col-md-10 col-xs-8'>" + block["gensigStr"] + "</div></div>"));
		
		this.data.empty();
	}
	
	addLine(message, id, time, type)
	{
		if (!type)
			type = "";
		
		this.data.prepend(getNewLine(message, id, time, type));
		//window.scrollTo(0, document.body.scrollHeight);
	}
}

var ws;
var system = $("#system");
var thisBlockBest;
var thisBlockBestElement = $("#thisBlockBest");
var bestDeadlineOverall;
var bestDeadlineOverallElement = $("#bestOverall");
var minedBlocks = -1;
var minedBlocksElement = $("#minedBlocks");
var name = document.title;
var confirmedDeadlines = 0;
var confirmedDeadlinesElement = $("#confirmedDeadlines");
var noncesFound = $("#cbNoncesFound");
var noncesSent = $("#cbNoncesSent");
var noncesConfirmed = $("#cbNoncesConfirmed");
var progressBar = $("#progressBar");
var lastWinnerContainer = $("#lastWinnerContainer");
var lastWinner = $("#lastWinner");
var confirmedSound = new Audio("sounds/sms-alert-1-daniel_simon.mp3");
var playConfirmationSound = true;
var iconConfirmationSound = $("#iconConfirmationSound");
var avgDeadline = $("#avgDeadline");
var connectionStatus = $("#connectionStatus");
var wonBlocks = $("#wonBlocks");
var bestDeadlinesChart = $("#deadlinesChart");
var plot;
var deadlinesInfo = $("#deadlinesInfo");
var block = new Block();

if (confirmedSound)
	confirmedSound.volume = 0.5;

function newBlock(json)
{
	block.newBlock(json);
	thisBlockBestElement.html(nullDeadline);
	setMinedBlocks(json["blocksMined"]);
	document.title = name + " (" + json["block"] + ")";
	checkAddBestOverall(json["bestOverallNum"], json["bestOverall"]);
	setConfirmedDeadlines(json["deadlinesConfirmed"]);
	setProgress(0);
	lastWinnerContainer.hide();
	avgDeadline.html(json["deadlinesAvg"]);
	wonBlocks.html(json["blocksWon"]);
	plot.setData([json["bestDeadlines"]]);
	plot.setupGrid();
	plot.draw();
	showDeadlinesInfo(null);
}

function getNewLine(message, id, time, type)
{
	if (type)
		type = " " + type;
	
	return "<li id='" + id + "' class='list-group-item clearfix" + type + "'>" +
		message + "<p class='pull-right'>" + time + "</p>" +
		"</li>";
}

function addMinedBlocks()
{
	minedBlocks += 1;
	minedBlocksElement.html(minedBlocks);
}

function setMinedBlocks(blocks)
{
	minedBlocks = blocks;
	minedBlocksElement.html(minedBlocks);
}
		
function nonceFound(deadline)
{
	if (noncesFound.prop("checked"))
	{
		block.addLine("<span class='glyphicon glyphicon-zoom-in' aria-hidden='true'></span> <b>" +
			deadline["account"] + "</b>: nonce found (" + deadline["deadline"] + ")",
			deadline["nonce"], deadline["time"], "nonce-found");
	}
}

function checkAddBestRound(deadlineNum, deadline)
{
	if (!thisBlockBest ||
		thisBlockBest > deadlineNum)
	{
		thisBlockBest = deadlineNum;
		thisBlockBestElement.html(deadline);
	}
}

function checkAddBestOverall(deadlineNum, deadline)
{
	if (!bestDeadlineOverall ||
		bestDeadlineOverall > deadlineNum)
	{
		bestDeadlineOverall = deadlineNum;
		bestDeadlineOverallElement.html(deadline);
	}
}
		
function addOrSubmit(deadline)
{
	var line = $("#" + deadline["nonce"]);
	var message = "<span class='glyphicon glyphicon-chevron-right' aria-hidden='true'></span> <b>";
	message += deadline["account"] + "</b>: nonce submitted (" + deadline["deadline"] + ")";
	message += "<p class='pull-right'>" + deadline["time"] + "</p>";
	
	if (noncesSent.prop("checked"))
	{
		if (line.length)
		{
			line.addClass("list-group-item-success");
			line.html(message);
		}
		else
		{
			addLine(message, deadline["nonce"], deadline["time"], "list-group-item-success");
		}
	}
}

function addOrConfirm(deadline)
{
	var line = $("#" + deadline["nonce"]);
	var message = "<b>" + deadline["account"] + "</b>: nonce confirmed (" + deadline["deadline"] + ")";
	message += "<p class='pull-right'>" + deadline["time"] + "</p>";
	
	if (noncesConfirmed.prop("checked"))
	{
		if (line.length)
		{
			line.addClass("list-group-item-success");
			line.html("<span class='glyphicon glyphicon-ok' aria-hidden='true'></span> "+ message);
		}
		else
		{
			addLine(message, deadline["nonce"], deadline["time"], "list-group-item-success");
		}
		
		if (confirmedSound && playConfirmationSound)
			confirmedSound.play();
	}
	
	setConfirmedDeadlines(confirmedDeadlines + 1);
}

function addSystemEntry(key, value)
{	
	var html = "<div class='row'>";
	html	+= "	<div class='col-md-5 col-xs-5'>" + key + "</div>";
	html	+= "	<div class='col-md-7 col-xs-7'>" + value + "</div>";
	html	+= "</div>";
	system.append(html);
}

function addLinkWithLabel(link)
{
	return "<a href='" + link + "' target='_blank'>" + link + "</a>";
}

function config(cfg)
{		
	system.html("");
	addSystemEntry("Pool-URL", addLinkWithLabel(cfg["poolUrl"]));
	addSystemEntry("Mining-URL", addLinkWithLabel(cfg["miningInfoUrl"]));
	addSystemEntry("Wallet-URL", addLinkWithLabel(cfg["walletUrl"]));
	addSystemEntry("Plotsize", cfg["totalPlotSize"]);
	addSystemEntry("Buffersize", cfg["bufferSize"]);
	addSystemEntry("Target deadline", cfg["targetDeadline"]);
	addSystemEntry("Plot readers", cfg["maxPlotReaders"]);
	addSystemEntry("Mining intensity", cfg["miningIntensity"]);
	addSystemEntry("Submission retry", cfg["submissionMaxRetry"]);
}

function setConfirmedDeadlines(deadlines)
{
	confirmedDeadlines = deadlines;
	confirmedDeadlinesElement.html(confirmedDeadlines);
}

function setProgress(value)
{
	var valueFixed = value.toFixed(0);
				
	if (valueFixed >= 100)
	{
		valueFixed = 100;
		progressBar.removeClass("active");
	}
	else if (valueFixed <= 100)
	{
		if (valueFixed < 0)
			valueFixed = 0;
		
		if (!progressBar.hasClass("active"))
			progressBar.addClass("active");
	}
	
	progressBar.attr("style", "width:" + valueFixed + "%");
	progressBar.html(valueFixed + " %");
}

function setLastWinner(winner)
{
	if (winner)
	{
		var burstAddress = winner["address"];
		var numeric = lastWinner.find("#lastWinnerNumeric");
		var address = lastWinner.find("#lastWinnerAddress");
		var name = lastWinner.find("#lastWinnerName");
		
		var link = "https://block.burstcoin.info/acc.php?acc=BURST-" + burstAddress;
	
		if (winner["name"])
		{
			name.html(winner["name"]);
			name.attr("href", link);
			lastWinner.find("#lastWinnerNameRow").show();
		}
		else
		{
			lastWinner.find("#lastWinnerNameRow").hide();
		}
	
		numeric.html(winner["numeric"]);				
		address.html(winner["address"]);
		
		numeric.attr("href", link);
		address.attr("href", link);
		
		lastWinnerContainer.show();
	}
	else
	{
		lastWinnerContainer.hide();
	}
}

function deActivateConfirmationSound(on)
{
	playConfirmationSound = on;

	iconConfirmationSound.removeClass("glyphicon-volume-up");
	iconConfirmationSound.removeClass("glyphicon-volume-off");

	if (on)
		iconConfirmationSound.addClass("glyphicon-volume-up");
	else
		iconConfirmationSound.addClass("glyphicon-volume-off");
}

function toggleConfirmationSound()
{
	deActivateConfirmationSound(!playConfirmationSound);
}

function connect()
{
	if ("WebSocket" in window)
	{
		if (ws)
			ws.close();
			
		ws = new WebSocket("ws://" + ip + ":" + port);
		
		ws.onopen = function(evt)
		{
			connectionStatus.html("connected");
		};
		
		ws.onmessage = function(msg)
		{
			data = msg["data"];
		
			if (data)
			{
				var response = JSON.parse(data);
				
				switch (response["type"])
				{
					case "new block":
						newBlock(response);
						break;
					case "nonce found":
						nonceFound(response);
						break;
					case "nonce confirmed":
						addOrConfirm(response);
						checkAddBestRound(response["deadlineNum"], response["deadline"]);
						checkAddBestOverall(response["deadlineNum"], response["deadline"]);
						break;
					case "nonce submitted":
						addOrSubmit(response);
						break;
					case "config":
						config(response);
						break;
					case "progress":
						setProgress(response["value"]);
						break;
					case "lastWinner":
						setLastWinner(response);
						break;
					case "blocksWonUpdate":
						wonBlocks.html(reponse["blocksWon"]);
						break;
					default:
						console.log(response["type"]);
						break;
				};
			}
		};
	}
	else
	{
		ws = null;
	}
}

function deadlineFormat(val)
{
	var secs = Math.floor(val);
	var mins = Math.floor(secs / 60);
	var hours = Math.floor(mins / 60);
	var day = Math.floor(hours / 24);
	var months = Math.floor(day / 30);
	var years = Math.floor(months / 12);
	var msg = "";
	
	if (years > 0)
		msg += years.toFixed() + "y ";
	if (months > 0)
		msg += (months % 12).toFixed() + "m ";
	if (day > 0)
		msg += day % 30 + "d ";
	msg += ("00" + (hours % 24)).slice(-2) + ':';
	msg += ("00" + (mins % 60)).slice(-2) + ':';
	msg += ("00" + (secs % 60)).slice(-2);

	return msg;
}

function showDeadlinesInfo(deadlineObj)
{	
	var infos = "---";
		
	plot.unhighlight();
	
	if (deadlineObj)
	{		
		var infos = "block <b>" + deadlineObj.datapoint[0] + "</b>: " +
			deadlineFormat(deadlineObj.datapoint[1]);

		plot.highlight(deadlineObj.series, deadlineObj.datapoint);
	}
	
	deadlinesInfo.html(infos);
	lastDeadlineInfo = deadlineObj;
}

function initPlot()
{
	var options = {
		series: {
			lines: {
				show: true,
				fill: true
			},
			points: { show: true }
		},
		grid: {
			hoverable: true,
			autoHighlight: true,
			clickable: true
		},
		xaxis: {
			show: false
		},
		yaxis: {
			mode: "time",
			min: 0,
			timeformat: "%Yy %mm %dd %H:%M:%S",
			tickFormatter: function (val, axis) {
				return deadlineFormat(val);
			}
		}
	};
	
	plot = $.plot(bestDeadlinesChart, [], options);
	
	bestDeadlinesChart.bind("plotclick", function (event, pos, item) {
		if (item)
			showDeadlinesInfo(item);
	});
	
	bestDeadlinesChart.bind("plothover", function (event, pos, item) {
		if (item)
			showDeadlinesInfo(item);
	});
}

window.onload = function(evt)
{
	initPlot();
	bestDeadlineOverallElement.html(nullDeadline);
	connect();
	deActivateConfirmationSound(true);
	showDeadlinesInfo(null);
	$("#container").html(block.panel);
}

window.onresize = function(evt)
{
	if (plot)
	{
		plot.resize(0, 0);
		plot.setupGrid();
		plot.draw();
	}
}
