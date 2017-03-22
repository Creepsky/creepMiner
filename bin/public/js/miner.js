class Deadline {
    constructor(deadline) {
        this.accountName = deadline["account"];
        this.accountId = BigInteger(deadline["accountId"]);
        this.deadlineStr = deadline["deadline"];
        this.deadline = BigInteger(deadline["deadlineNum"]);
        this.nonce = BigInteger(deadline["nonce"]);
        this.time = deadline["time"];
        this.plotfile = deadline["plotfile"];
    }
}

class Block {
	constructor() {
		this.panel = $("<div class='panel panel-primary'></div>");
		this.head = $("<div class='panel-heading'></div>");
		this.body = $("<div class='panel-body'></div>");
		this.data = $("<ul id='blkdata' class='list-group'></ul>");
				
		this.panel.append(this.head);
		this.panel.append(this.body);
		this.panel.append(this.data);
		
		this.bestDeadline = null;
	}
	
	newBlock(block)	{
		this.head.html("Block " + block["block"]);
		this.body.html("<div class='row'><div class='col-md-3 col-xs-5'>Time</div><div class='col-md-9 col-xs-7'>" + block["time"] + "</div></div>");
		this.body.append($("<div class='row'><div class='col-md-3 col-xs-5'>Scoop</div><div class='col-md-9 col-xs-7'>" + block["scoop"] + "</div></div>"));
		this.body.append($("<div class='row'><div class='col-md-3 col-xs-5'>Base target</div><div class='col-md-9 col-xs-7'>" + block["baseTarget"] + "</div></div>"));
		//this.body.append($("<div class='row'><div class='col-md-2 col-xs-4'>Gensignature</div><div class='col-md-10 col-xs-8'>" + block["gensigStr"] + "</div></div>"));
		
		this.data.empty();
	}
	
	addLine(line) {
		this.data.prepend(line);
		//window.scrollTo(0, document.body.scrollHeight);
	}

	getLine(id) {
	    return this.data.find("#" + id);
	}
	
	getData() {
		return this.data;
	}
}

var system;
var thisBlockBest;
var thisBlockBestElement;
var bestDeadlineOverall;
var bestDeadlineOverallElement;
var minedBlocks = -1;
var minedBlocksElement;
var name = document.title;
var confirmedDeadlines = 0;
var confirmedDeadlinesElement;
var hideSameNonces;
var noncesFound;
var noncesSent;
var noncesConfirmed;
var progressBar;
var lastWinnerContainer;
var lastWinner;
var confirmedSound = new Audio("sounds/sms-alert-1-daniel_simon.mp3");
var playConfirmationSound = true;
var iconConfirmationSound;
var avgDeadline;
var wonBlocks;
var bestDeadlinesChart;
var plot;
var deadlinesInfo;
var miningData = new Block();
var settingsDlComboboxes;
var logSettings = {};

if (confirmedSound)
	confirmedSound.volume = 0.5;

var NONCE_FOUND = 1 << 0;
var NONCE_SENT = 1 << 1;
var NONCE_CONFIRMED = 1 << 2;

var loggers = [
	[ "miner", "Miner", 6 ],
	[ "config", "Config", 6 ],
	[ "server", "Server", 1 ],
	[ "socket", "Socket", 0 ],
	[ "session", "Session", 3 ],
	[ "nonceSubmitter", "Nonce submitter", 6 ],
	[ "plotReader", "Plot reader", 6 ],
	[ "plotVerifier", "Plot verifier", 6 ],
	[ "wallet", "Wallet", 6 ],
	[ "general", "General", 6 ]
];

var levels = [
	"off", "fatal", "critical", "error", "warning",
	"notice", "information", "debug",  "trace", "all"
];

function newBlock(json) {
	miningData.newBlock(json);
	thisBlockBest = null;
	thisBlockBestElement.html(nullDeadline);
	setMinedBlocks(json["blocksMined"]);
	document.title = name + " (" + json["block"] + ")";
	checkAddBestOverall(BigInteger(json["bestOverallNum"]), json["bestOverall"]);
	setConfirmedDeadlines(BigInteger(json["deadlinesConfirmed"]));
	setOverallProgress(0);
	lastWinnerContainer.hide();
	avgDeadline.html(json["deadlinesAvg"]);
	wonBlocks.html(json["blocksWon"]);
	plot.setData([json["bestDeadlines"]]);
	plot.setupGrid();
	plot.draw();
	showDeadlinesInfo(null);
}

function getNewLine(type, id) {
	var line = $("<li class='list-group-item clearfix'></li>");
	
	if (id)
		line.attr("id", id);

    if (type)
        line.addClass(type);

	return line;
}

function addMinedBlocks() {
	minedBlocks += 1;
	minedBlocksElement.html(minedBlocks);
}

function setMinedBlocks(blocks) {
	minedBlocks = blocks;
	minedBlocksElement.html(minedBlocks);
}

function createNonceLine(deadline, glyphIconId, lineType, nonceType) {
    var line = getNewLine(lineType, deadline.nonce);

	var hiddenInfosDiv = $("<small></small>");
    var hiddenInfos = $("<dl class='dl'></dl>");
    hiddenInfos.append("<dt>Nonce</dt><dd>" + deadline.nonce + "</dd>");
    hiddenInfos.append("<dt>Plotfile</dt><dd>" + deadline.plotfile + "</dd>");
    hiddenInfos.hide();

	hiddenInfosDiv.append(hiddenInfos);
	
    var message = $("<div></div>");

    line.click(function () {
        hiddenInfos.toggle();
    });

	var nonceTypeStr = "";
	
	if (nonceType == NONCE_FOUND)
		nonceTypeStr = "nonce found";
	else if (nonceType == NONCE_SENT)
		nonceTypeStr = "nonce submitted";
	else if (nonceType == NONCE_CONFIRMED)
		nonceTypeStr = "nonce confirmed";
	
    message.append("<span class='glyphicon " + glyphIconId + "' aria-hidden='true'></span> ");
    message.append("<b><a href='https://block.burstcoin.info/acc.php?acc=" + deadline.accountId + "' target='_blank'>" +
		deadline.accountName + "</a></b>: " + nonceTypeStr);
    message.append(" (" + deadline.deadlineStr + ")");
    message.append("<p class='pull-right'><span class='label label-default'>" + deadline.time + "</span></p>");

    message.append(hiddenInfosDiv);

    line.html(message);
	line.attr("logger", "nonceSubmitter");
	line.attr("level", 6);
	line.attr("nonce", deadline.nonce);
	line.attr("nonceType", nonceType);

    return line;
}

function createMessageLine(lineType, logger, level, file, lineNumber, time, text, hidden) {
	var line = getNewLine(lineType);
	
	var hiddenInfosDiv = $("<small></small>");
	var hiddenInfos = $("<dl class='dl'></dl>");
    hiddenInfos.append("<dt>Logger</dt><dd>" + logger + "</dd>");
    hiddenInfos.append("<dt>File</dt><dd>" + file + "</dd>");
    hiddenInfos.append("<dt>Line</dt><dd>" + lineNumber + "</dd>");
    hiddenInfos.hide();
	
	hiddenInfosDiv.append(hiddenInfos);
	
	line.click(function () {
        hiddenInfos.toggle();
    });
	
	var message = $("<div></div>");
	message.append("<p class='pull-right'><span class='label label-default'>" + time + "</span></p>");
	message.append(text);
	message.append(hiddenInfosDiv);
	
	line.html(message);
	
	if (hidden)
		line.hide();
	
	line.attr("logger", logger);
	line.attr("level", level);
	
	return line;
}

function checkAddBestRound(deadlineNum, deadline) {	
	if (!thisBlockBest ||
		thisBlockBest > deadlineNum) {
		thisBlockBest = deadlineNum;
		thisBlockBestElement.html(deadline);
	}
}

function checkAddBestOverall(deadlineNum, deadline) {
	if (!bestDeadlineOverall ||
		bestDeadlineOverall > deadlineNum) {
		bestDeadlineOverall = deadlineNum;
		bestDeadlineOverallElement.html(deadline);
	}
}

function replaceOrAddNonceLine(json, glyphIcon, lineType, message) {
    var deadline = new Deadline(json);
    //var line = miningData.getLine(deadline.nonce);
    var newLine = createNonceLine(deadline, glyphIcon, lineType, message);

    //if (line.length)
    //     line.replaceWith(newLine);
    //else
        miningData.addLine(newLine);
	
	hideSameNonceLines(deadline.nonce);
	
    return newLine;
}

function hideSameNonceLines(nonce) {
	var doHideSameNonces = hideSameNonces.is(":checked");
	var highest = -1;
	var highestElement = null;
	
	var nonceTypeChecked = [];

	if (noncesFound.prop("checked"))
		nonceTypeChecked.push(NONCE_FOUND);

	if (noncesSent.prop("checked"))
		nonceTypeChecked.push(NONCE_SENT);
		
	if (noncesConfirmed.prop("checked"))
		nonceTypeChecked.push(NONCE_CONFIRMED);
		
	var sameLines = miningData.getData().find("[nonce='" + nonce + "']").each(function(){		
		var nonceTypeStr = $(this).attr("nonceType");
		var nonceType = null;
		
		if (nonceTypeStr)
			nonceType = parseInt(nonceTypeStr);
		
		if (nonceType && nonceTypeChecked.indexOf(nonceType) != -1) {
			if (doHideSameNonces) {
				$(this).hide();

				if (nonceType > highest) {
					highest = nonceType;
					highestElement = $(this);
				}
			}
			else {
				$(this).show();
			}
		}
		else {
			$(this).hide();
		}
	});
	
	if (highestElement)
		highestElement.show();
}

function nonceFound(json) {
    if (noncesFound.prop("checked"))
        replaceOrAddNonceLine(json, "glyphicon-zoom-in", "list-group-item-default", NONCE_FOUND);
}

function addOrSubmit(json) {
    if (noncesSent.prop("checked"))
        replaceOrAddNonceLine(json, "glyphicon-share-alt", "list-group-item-success", NONCE_SENT);
}

function addOrConfirm(json) {
    if (noncesConfirmed.prop("checked")) {
        replaceOrAddNonceLine(json, "glyphicon-ok", "list-group-item-success", NONCE_CONFIRMED);

		if (confirmedSound && playConfirmationSound)
			confirmedSound.play();
	}
	
	setConfirmedDeadlines(confirmedDeadlines + 1);
}

function addSystemEntry(key, value) {	
	var html = "<div class='row'>";
	html	+= "	<div class='col-md-5 col-xs-5'>" + key + "</div>";
	html	+= "	<div class='col-md-7 col-xs-7'>" + value + "</div>";
	html	+= "</div>";
	system.append(html);
}

function addLinkWithLabel(link) {
	return "<a href='" + link + "' target='_blank'>" + link + "</a>";
}

function config(cfg) {
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

function createLoggerCombobox(cmb) {
	cmb.empty();
	
	levels.forEach(function(element, index, array) {
		cmb.append("<option value=" + index + ">" + element + "</option>");
	});
}

function initSettings() {
	loggers.forEach(function(element, index, array) {
		element[1][0].toUpperCase();
		var cmb = $("<select id='cmb" + element[0] + "' class='selectpicker form-control'></select>");
		createLoggerCombobox(cmb);
		cmb.change(function(){
			reparseMessages();
		});
		
		var div = $("<div class='form-group input-group'></div>");
		var label = $("<span class='input-group-addon'>" + element[1] + "</span>");
				
		div.append(label);
		div.append(cmb);
		
		cmb.val(element[2]);
		logSettings[element[0]] = cmb;
		
		settingsDlComboboxes.append(div);
	});
}

function setConfirmedDeadlines(deadlines) {
	confirmedDeadlines = deadlines;
	confirmedDeadlinesElement.html(confirmedDeadlines);
}

function setOverallProgress(value) {
	setProgress(progressBar, value);
}

function setLastWinner(winner) {
	if (winner) {
		var burstAddress = winner["address"];
		var numeric = lastWinner.find("#lastWinnerNumeric");
		var address = lastWinner.find("#lastWinnerAddress");
		var name = lastWinner.find("#lastWinnerName");
		
		var link = "https://block.burstcoin.info/acc.php?acc=BURST-" + burstAddress;
	
		if (winner["name"]) {
			name.html(winner["name"]);
			name.attr("href", link);
			lastWinner.find("#lastWinnerNameRow").show();
		}
		else {
			lastWinner.find("#lastWinnerNameRow").hide();
		}
	
		numeric.html(winner["numeric"]);				
		address.html(winner["address"]);
		
		numeric.attr("href", link);
		address.attr("href", link);
		
		lastWinnerContainer.show();
	}
	else {
		lastWinnerContainer.hide();
	}
}

function deActivateConfirmationSound(on) {
	playConfirmationSound = on;

	iconConfirmationSound.removeClass("glyphicon-volume-up");
	iconConfirmationSound.removeClass("glyphicon-volume-off");

	if (on)
		iconConfirmationSound.addClass("glyphicon-volume-up");
	else
		iconConfirmationSound.addClass("glyphicon-volume-off");
}

function toggleConfirmationSound() {
	deActivateConfirmationSound(!playConfirmationSound);
}

function showMessage(json) {
	var type = json["type"];
	var text = json["text"];
	var logger = json["source"];
	
	if (type && text && logger) {
		/*
		0 : all
		1 : fatal
		2 : critical
		3 : error
		4 : warning
		5 : notice
		6 : information
		7 : debug
		8 : trace
		9 : off
		*/
		
		var hidden = type > logSettings[logger].val();
		var lineType;
		
		switch (parseInt(type)) {
			case 1:
			case 2:
			case 3:
				lineType = "list-group-item-danger";
				break;
			case 4:
				lineType = "list-group-item-warning";
				break;
			case 5:
			case 6:
			case 7:
			case 8:
				lineType = "list-group-item-debug";
				break;
			default:
				lineType = "list-group-item-default";
				break;
		};
		
		var newLine = createMessageLine(lineType, logger, parseInt(type), json["file"], json["line"],
			json["time"], json["text"].replace(new RegExp('\r?\n','g'), '<br />'), hidden);
				
		miningData.addLine(newLine);
	}
	else
		console.log("unknown message type: " + json);
}

function reparseMessages() {
	var nonces = [];
	
	miningData.getData().find("li").each(function(){
		var logger = $(this).attr("logger");
		var levelStr = $(this).attr("level");
		var nonce = $(this).attr("nonce");

		if (nonce) {
			if (nonces.indexOf(nonce) == -1) {
				nonces.push(nonce);
				hideSameNonceLines(nonce);
			}
		}
		else if (logger && levelStr) {
			var loggerLevel = parseInt(logSettings[logger].val());
			var level = parseInt(levelStr);
			
			if (level <= loggerLevel)
				$(this).show();
			else
				$(this).hide();
		}
	});
}

function resetLogSettings() {
	hideSameNonces.prop('checked', true);
	noncesFound.prop('checked', true);
	noncesSent.prop('checked', true);
	noncesConfirmed.prop('checked', true);
	
	loggers.forEach(function(element, index, array) {
		logSettings[element[0]].val(element[2]);
	});
}

function connectBlock() {	
	connect(function(msg) {
		data = msg["data"];
	
		if (data) {
			if (data == "ping")
				return;
			
			var response = JSON.parse(data);
			
			switch (response["type"]) {
				case "new block":
					newBlock(response);
					break;
				case "nonce found":
					nonceFound(response);
					break;
				case "nonce confirmed":
					addOrConfirm(response);
					checkAddBestRound(BigInteger(response["deadlineNum"]), response["deadline"]);
					checkAddBestOverall(BigInteger(response["deadlineNum"]), response["deadline"]);
					break;
				case "nonce submitted":
					addOrSubmit(response);
					break;
				case "config":
					config(response);
					break;
				case "progress":
					setOverallProgress(response["value"]);
					break;
				case "lastWinner":
					setLastWinner(response);
					break;
				case "blocksWonUpdate":
					wonBlocks.html(reponse["blocksWon"]);
					break;
				case "plotdir-progress":
				case "plotfiles-rescan":
					// do nothing
					break;
				default:
					showMessage(response);
					break;
			};
		}
	});
}

function deadlineFormat(val) {
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

function showDeadlinesInfo(deadlineObj) {	
	var infos = "---";
		
	plot.unhighlight();
	
	if (deadlineObj) {	
		var infos = "block <b>" + deadlineObj.datapoint[0] + "</b>: " +
			deadlineFormat(deadlineObj.datapoint[1]);

		plot.highlight(deadlineObj.series, deadlineObj.datapoint);
	}
	
	deadlinesInfo.html(infos);
	lastDeadlineInfo = deadlineObj;
}

function initBlock() {
    system = $("#system");
    thisBlockBestElement = $("#thisBlockBest");
    bestDeadlineOverallElement = $("#bestOverall");
    minedBlocksElement = $("#minedBlocks");
    confirmedDeadlinesElement = $("#confirmedDeadlines");
	hideSameNonces = $("#cbHideSameNonces");
    noncesFound = $("#cbNoncesFound");
    noncesSent = $("#cbNoncesSent");
    noncesConfirmed = $("#cbNoncesConfirmed");
    progressBar = $("#progressBar");
    lastWinnerContainer = $("#lastWinnerContainer");
    lastWinner = $("#lastWinner");
    iconConfirmationSound = $("#iconConfirmationSound");
    avgDeadline = $("#avgDeadline");
    connectionStatus = $("#connectionStatus");
    wonBlocks = $("#wonBlocks");
    bestDeadlinesChart = $("#deadlinesChart");
    deadlinesInfo = $("#deadlinesInfo");
	settingsDlComboboxes = $("#settingsDlComboboxes");
	
    initPlot();
    bestDeadlineOverallElement.html(nullDeadline);
    connectBlock();
    deActivateConfirmationSound(true);
    showDeadlinesInfo(null);
	$("#container").append(miningData.panel);
	
	hideSameNonces.change(reparseMessages);
	noncesFound.change(reparseMessages);
	noncesSent.change(reparseMessages);
	noncesConfirmed.change(reparseMessages);
	
	initSettings();
}

function initPlot() {
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
	
	plot = $.plot($("#deadlinesChart"), [], options);
	
	$("#deadlinesChart").bind("plotclick", function (event, pos, item) {
		if (item)
			showDeadlinesInfo(item);
	});
	
	$("#deadlinesChart").bind("plothover", function (event, pos, item) {
		if (item)
			showDeadlinesInfo(item);
	});
}

window.onresize = function(evt) {
	if (plot) {
		plot.resize(0, 0);
		plot.setupGrid();
		plot.draw();
	}
}

window.onload = function(evt) {
	$("#btnBlock").addClass('active');
	initBlock();
}
