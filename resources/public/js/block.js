class Deadline {
    constructor(deadline) {
        this.accountName = deadline["account"];
        this.accountId = BigInteger(deadline["accountId"]);
        this.accountIdStr = deadline["accountId"];
        this.deadlineStr = deadline["deadline"];
        this.deadline = BigInteger(deadline["deadlineNum"]);
        this.nonce = BigInteger(deadline["nonce"]);
        this.time = deadline["time"];
        this.plotfile = deadline["plotfile"];
    }
}

class Block {
    constructor() {
        this.panel = $("<div class='card mb-12'></div>");
        this.head = $("<h4 class='card-header bg-success text-white'>");
        this.body = $("<div class='card-body'></div>");
        this.data = $("<ul id='blkdata' class='list-group'></ul>");

        this.panel.append(this.head);
        this.panel.append(this.body);
        this.panel.append(this.data);

        this.bestDeadline = null;
    }

    newBlock(block) {
        this.head.html("Current block: " + block["block"] + "</h4><span class='badge badge-light float-sm-right' id='blockTimer'></span>");
        this.body.html("<li class='list-group-item d-flex justify-content-between align-items-center' style='border:none; padding:0'>Start time <span class='badge badge-secondary float-sm-right'>" + block["time"] + "</span></li>");
        this.body.append($("<li class='list-group-item d-flex justify-content-between align-items-center' style='border:none; padding:0'>Scoop <span>" + block["scoop"] + "</span></li>"));
        this.body.append($("<li class='list-group-item d-flex justify-content-between align-items-center' style='border:none; padding:0'>Base target<span>" + block["baseTarget"] + "</span></li>"));



        blockStartTime=block["startTime"];
        var diffDifference = block['difficultyDifference'];
        var diffDifferenceString = String(diffDifference);

        if (diffDifference === 0)
            diffDifferenceString = "no change";
        else if (diffDifference > 0)
            diffDifferenceString = "+" + String(diffDifference);

        this.body.append($("<li class='list-group-item d-flex justify-content-between align-items-center' style='border:none; padding:0'>Difficulty <span>" + block["difficulty"] + " (" + diffDifferenceString + ")</span></li>"));
        this.body.append($("<li class='list-group-item d-flex justify-content-between align-items-center' style='border:none; padding:0'>Target <span class='badge badge-primary badge-pill'>" + deadlineFormat(block["targetDeadline"]) + "</span></li>"));

        //this.body.append($("<li class='list-group-item d-flex justify-content-between align-items-center'>Generation signature</li>"));
        //this.body.append($("<li class='list-group-item d-flex justify-content-between align-items-center'><small>" + block["gensigStr"] + "</small></li>"));
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

$('#timePlotButton').on('click', function(event) {
    if (timePlotMax == maxBlockTime*1.25)
        timePlotMax = maxScanTime*1.25;
    else
        timePlotMax = maxBlockTime*1.25;
    if (timePlotMax < 30) timePlotMax = 30;
    timePlot.getAxes().yaxis.options.max = timePlotMax;
    timePlot.setupGrid();
    timePlot.draw();
});

$('#deadlinePlotButton').on('click', function(event) {
    if (deadlinePlotMax)
        deadlinePlotMax = null;
    else
        deadlinePlotMax = deadlinePlotSmallMax;
    deadlinePlot.getAxes().yaxis.options.max = deadlinePlotMax;
    deadlinePlot.setupGrid();
    deadlinePlot.draw();
});

var timerRefresh = setInterval(function(){ myTimer() }, 1000);

function myTimer() {
    var d = new Date() / 1000;
    var t = Math.round(d - blockStartTime);
    var timerElement=document.getElementById("blockTimer");
    if (timerElement)
        timerElement.innerHTML = deadlineFormat(t);
}

var system;
var bestDeadlineOverall;
var bestDeadlineOverallElement;
var bestHistorical;
var minedBlocks = -1;
var minedBlocksElement;
var name = document.title;
var hideSameNonces;
var noncesFound;
var noncesSent;
var noncesConfirmed;
var progressBar;
var progressBarVerify;
var lastWinnerContainer;
var lastWinner;
var confirmedSound = new Audio("sounds/alert.mp3");
var playConfirmationSound = true;
var iconConfirmationSound;
var avgDeadline;
var wonBlocks;
var lowestDiff;
var highestDiff;
var deadlinePlotSmallMax=86400;
var deadlinePlotMax=null;
var bestDeadlinesChart;
var deadlinePlot;
var deadlinesInfo;
var maxScanTime=10;
var maxBlockTime=10;
var timePlotMax=10;
var timeChart;
var timePlot;
var timeInfo;
var deadlineDistributionChart;
var deadlineDistributionBarWidth = 1;
var deadlineDistributionPlot;
var deadlineDistributionInfo;
var difficultyChart;
var difficultyPlot;
var difficultyInfo;
var miningData = new Block();
var settingsDlComboboxes;
var maxHistoricalBlocks;
var blockStartTime = new Date() / 1000;

// ******************************************
var logSettings = {};
// ******************************************

// I change this [] to {} and also in settings.js and general.js

// var logSettings = [];

if (confirmedSound)
    confirmedSound.volume = 0.5;

var NONCE_FOUND = 1 << 0;
var NONCE_SENT = 1 << 1;
var NONCE_CONFIRMED = 1 << 2;



// ******************************************

// Set on click event of elements

$("#cbHideSameNonces").on('click', function () {
    localSet('hideSameNonces', this.checked);
})

$("#cbNoncesFound").on('click', function () {
    localSet('noncesFound', this.checked);
})

$("#cbNoncesSent").on('click', function () {
    localSet('noncesSent', this.checked);
})

$("#cbNoncesConfirmed").on('click', function () {
    localSet('noncesConfirmed', this.checked);
})



// Retrieve any possible local variable and set in js vars

function localInitCheckBoxes() {
    playConfirmationSound = localGet('playConfirmationSound');

    // Check if values exist in local or set true(default)

    var temp = localGet('hideSameNonces');

    if (temp == null) temp = true;
    hideSameNonces.prop('checked', temp);

    temp = localGet('noncesFound');
    if (temp == null) temp = true;
    noncesFound.prop('checked', temp);

    temp = localGet('noncesSent');
    if (temp == null) temp = true;
    noncesSent.prop('checked', temp);

    temp = localGet('noncesConfirmed');
    if (temp == null) temp = true;
    noncesConfirmed.prop('checked', temp);
}

// I break in 2 functions because must initialized in different points of initBlock

function localInitLogSettings() {
    var temp = localGet('logSettings');

    if (temp == null)
        return;

    if (temp['miner']) $("#cmb_miner").val(temp['miner'])
    if (temp['config']) $("#cmb_config").val(temp['config'])
    if (temp['server']) $("#cmb_server").val(temp['server'])
    if (temp['socket']) $("#cmb_socket").val(temp['socket'])
    if (temp['session']) $("#cmb_session").val(temp['session'])
    if (temp['nonceSubmitter']) $("#cmb_nonceSubmitter").val(temp['nonceSubmitter'])
    if (temp['plotReader']) $("#cmb_plotReader").val(temp['plotReader'])
    if (temp['plotVerifier']) $("#cmb_plotVerifier").val(temp['plotVerifier'])
    if (temp['wallet']) $("#cmb_wallet").val(temp['wallet'])
    if (temp['general']) $("#cmb_general").val(temp['general'])
}

// Saves in local storage - no expiration
// Item : name of local variable
// Data : content of item variable

function localSet(item, data) {
    localStorage.setItem(item, JSON.stringify(data));
}

// Get from local storage
// Item : name of local variable

function localGet(item) {
    return JSON.parse(localStorage.getItem(item));
}


// ******************************************


function newBlock(json) {
    miningData.newBlock(json);
    setMinedBlocks(json["blocksMined"]);
    document.title = name + " (" + json["block"] + ")";
    checkAddBestOverall(BigInteger(json["bestOverall"]["deadlineNum"]),
                                   json["bestOverall"]["deadline"],
                                   json["bestOverall"]["blockheight"]);

    var bestHistoricalJson = json["bestHistorical"]["deadline"];
    var bestHistoricalHeightJson = json["bestHistorical"]["blockheight"];

    if (bestHistoricalJson != null && bestHistoricalHeightJson != null)
        bestHistorical.html(bestHistoricalJson + " <small>@" + bestHistoricalHeightJson + "</small>");

    setOverallProgress(0);
    setOverallProgressVerify(0);
    lastWinnerContainer.hide();
    avgDeadline.html(json["deadlinesAvg"]);
    deadlinePerformance.html(Math.round(json["deadlinePerformance"]*1000)/1000 + " TB");
    var roundsSub=json["nRoundsSubmitted"]
    var numHistor=json["numHistoricals"]
    roundsSubmitted.html("<small>" + Math.round(roundsSub/numHistor*1000)/10 +"%</small>&nbsp;" + roundsSub + "/" + numHistor);
    wonBlocks.html(json["blocksWon"]);
    lowestDiff.html("<small>@" + json["lowestDifficulty"]["blockheight"] + "</small>&nbsp;&nbsp;" + json["lowestDifficulty"]["value"]);
    highestDiff.html("<small>@" + json["highestDifficulty"]["blockheight"] + "</small>&nbsp;&nbsp;" + json["highestDifficulty"]["value"]);
    meanDiff.html(Math.round(json["meanDifficulty"]));
    maxRoundTime.html(Math.round(json["maxRoundTime"]*1000)/1000 + " s");
    avgRoundTime.html(Math.round(json["meanRoundTime"]*1000)/1000 + " s");
    avgBlockTime.html(Math.round(json["meanBlockTime"]*1000)/1000 + " s");
    deadlinePlotSmallMax = -Math.log(0.95)*240*json["meanDifficulty"]/json["deadlinePerformance"];
    deadlinePlot.setData([json["bestDeadlines"]]);
    deadlinePlot.setupGrid();
    deadlinePlot.draw();
    maxBlockTime=json["maxBlockTime"];
    maxScanTime=json["maxRoundTime"];
    timePlotMax=maxBlockTime*1.25;
    initTimePlot();
    timePlot.setData([  {data: json["roundTimeHistory"], label:"&nbsp;&nbsp;<b>Scan time</b>&nbsp;&nbsp;&nbsp;"},
                        {data: json["blockTimeHistory"], label:"&nbsp;&nbsp;<b>Block time</b>",lines:{show:false},points:{show:true}}]);
    timePlot.setupGrid();
    timePlot.draw();
    deadlineDistributionBarWidth = json["dlDistBarWidth"]*0.99;
    initDeadlineDistributionPlot();
    deadlineDistributionPlot.setData([json["deadlineDistribution"]]);
    deadlineDistributionPlot.setupGrid();
    deadlineDistributionPlot.draw();
    difficultyPlot.setData([json["difficultyHistory"]]);
    difficultyPlot.setupGrid();
    difficultyPlot.draw();
    showDeadlinesInfo(null);
    showDeadlineDistributionInfo(null);
    showDifficultyInfo(null);
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
    hiddenInfos.append("<span class='badge badge-secondary float-sm-right'>" + deadline.time + "</span>");
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

    message.append("<span class='fa " + glyphIconId + "'></span> ");
    message.append("<b><a href='https://explore.burst.cryptoguru.org/account/" + deadline.accountId.toString() + "' target='_blank'>" +
        deadline.accountName + "</a></b>:&nbsp;" + nonceTypeStr + "&nbsp;");
    message.append("<span class='badge badge-primary badge-pill'>" + deadline.deadlineStr + "</span>");


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

function checkAddBestOverall(deadlineNum, deadline, blockheight) {
    if (deadline != null && (!bestDeadlineOverall || bestDeadlineOverall > deadlineNum)) {
        bestDeadlineOverall = deadlineNum;
        bestDeadlineOverallElement.html("<small>@" + blockheight + "</small>&nbsp;&nbsp;<span class='badge badge-primary badge-pill'>" + deadline + "</span>");
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

    var sameLines = miningData.getData().find("[nonce='" + nonce + "']").each(function () {
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
        replaceOrAddNonceLine(json, "fa fa-compass", "list-group-item-default", NONCE_FOUND);
}

function addOrSubmit(json) {
    if (noncesSent.prop("checked"))
        replaceOrAddNonceLine(json, "fa-paper-plane", "list-group-item-success", NONCE_SENT);
}

function addOrConfirm(json) {
    if (noncesConfirmed.prop("checked")) {
        replaceOrAddNonceLine(json, "fa-check", "list-group-item-success", NONCE_CONFIRMED);

        if (confirmedSound && playConfirmationSound)
            confirmedSound.play();
    }
}

function addLinkWithLabel(label, link) {
    return "<a href='" + link + "' target='_blank'>" + label + "</a>";
}

function config(cfg) {
    maxHistoricalBlocks = cfg["maxHistoricalBlocks"];
    $("#poolURL").html(addLinkWithLabel(cfg['poolUrl'] + ':' + cfg['poolUrlPort'], cfg["poolUrl"]));
    $("#miningURL").html(addLinkWithLabel(cfg["miningInfoUrl"] + ':' + cfg["miningInfoUrlPort"], cfg["miningInfoUrl"]));
    $("#walletURL").html(addLinkWithLabel(cfg["walletUrl"] + ':' + cfg["walletUrlPort"], cfg["walletUrl"] + ":" + cfg["walletUrlPort"]));
    $("#plotSize").html(cfg["totalPlotSize"]);
    if (cfg["submitProbability"] != 0)
        $("#submitProbDL").html("Submit probability<div>"+cfg["submitProbability"]+"</div>");
    else
        $("#submitProbDL").html("Target DL<span class='badge badge-primary badge-pill'><div>"+cfg["targetDeadlineCombined"]+"</div></span>");
    $("#poolDL").html(cfg["targetDeadlinePool"]);
    $("#readers").html(cfg["maxPlotReaders"]);
    $("#intensity").html(cfg["miningIntensity"]);
    $("#bufferSize").html(cfg["bufferSize"]);
}


function setOverallProgress(value) {
    setProgress(progressBar, value);
}

function setOverallProgressVerify(valueVerify) {
    setProgressVerify(progressBarVerify, valueVerify);
}

function setLastWinner(winner) {
    if (winner) {
        var burstAddress = winner["address"];
        var numeric = lastWinner.find("#lastWinnerNumeric");
        var address = lastWinner.find("#lastWinnerAddress");
        var name = lastWinner.find("#lastWinnerName");

        var link = "https://explore.burst.cryptoguru.org/account/BURST-" + burstAddress;

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

    iconConfirmationSound.removeClass("fa-volume-up");
    iconConfirmationSound.removeClass("fa-volume-off");

    if (on)
        iconConfirmationSound.addClass("fa-volume-up");
    else
        iconConfirmationSound.addClass("fa-volume-off");
}

function toggleConfirmationSound() {
    // ******************************************
    localSet('playConfirmationSound', !playConfirmationSound);
    // ******************************************

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
            json["time"], json["text"].replace(new RegExp('\r?\n', 'g'), '<br />'), hidden);

        miningData.addLine(newLine);
    }
    else
        console.log("unknown message type: " + json);
}

function reparseMessages() {
    var nonces = [];

    miningData.getData().find("li").each(function () {
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

    // ******************************************
    var tempSettings = {
        miner: logSettings['miner'].val(),
        config: logSettings['config'].val(),
        server: logSettings['server'].val(),
        socket: logSettings['socket'].val(),
        session: logSettings['session'].val(),
        nonceSubmitter: logSettings['nonceSubmitter'].val(),
        plotReader: logSettings['plotReader'].val(),
        plotVerifier: logSettings['plotVerifier'].val(),
        wallet: logSettings['wallet'].val(),
        general: logSettings['general'].val(),
    }

    localSet('logSettings', tempSettings);
    // ******************************************
}

function resetLogSettings() {
    hideSameNonces.prop('checked', true);
    noncesFound.prop('checked', true);
    noncesSent.prop('checked', true);
    noncesConfirmed.prop('checked', true);

    loggers.forEach(function (element, index, array) {
        logSettings[element[0]].val(element[2]);
    });
}

function connectBlock() {
    connect(function (msg) {
        data = msg["data"];

        if (data) {
            if (data == "ping")
                return;

            var response = JSON.parse(data);

            switch (response["type"]) {
                case "new block":
                    newBlock(response);
                    checkVersion(response["runningVersion"], response["onlineVersion"]);
                    break;
                case "nonce found":
                case "nonce found (too high)":
                    nonceFound(response);
                    break;
                case "nonce confirmed":
                    addOrConfirm(response);
                    checkAddBestOverall(BigInteger(response["deadlineNum"]), response["deadline"]);
                    break;
                case "nonce submitted":
                    addOrSubmit(response);
                    break;
                case "config":
                    config(response);
                    break;
                case "progress":
                    setOverallProgress(response["value"] - response["valueVerification"]);
                    setOverallProgressVerify(response["valueVerification"]);
                    break;
                case "lastWinner":
                    setLastWinner(response);
                    break;
                case "blocksWonUpdate":
                    wonBlocks.html(reponse["blocksWon"]);
                    break;
                case "plotdir-progress":
                case "plotdirs-rescan":
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

function deadlineFormatPlot(val) {
    var secs = Math.floor(val);
    var mins = Math.floor(secs / 60);
    var hours = Math.floor(mins / 60);
    var day = Math.floor(hours / 24);
    var months = Math.floor(day / 30);
    var years = Math.floor(months / 12);
    var msg = "";

    if (years > 0)
    {
        msg += years.toFixed() + "y ";
        msg += (months % 12).toFixed() + "m ";
        return msg;
    }
    if (months > 0)
    {
        msg += (months % 12).toFixed() + "m ";
        msg += day % 30 + "d ";
        return msg;
    }
    if (day > 0)
    {
        msg += day % 30 + "d ";
        msg += (hours % 24) + "h ";
        return msg;
    }
    msg += ("00" + (hours % 24)).slice(-2) + ':';
    msg += ("00" + (mins % 60)).slice(-2) + ':';
    msg += ("00" + (secs % 60)).slice(-2);

    return msg;
}


function showDeadlinesInfo(deadlineObj) {
    var infos = "---";

    deadlinePlot.unhighlight();

    if (deadlineObj) {
        var infos = "block <b>" + deadlineObj.datapoint[0] + "</b>: " +
            deadlineFormat(deadlineObj.datapoint[1]);

        deadlinePlot.highlight(deadlineObj.series, deadlineObj.datapoint);
    }

    deadlinesInfo.html(infos);
    lastDeadlineInfo = deadlineObj;
}

function showTimeInfo(timeObj) {
    var infos = "---";

    timePlot.unhighlight();

    if (timeObj) {
        var infos = "block <b>" + timeObj.datapoint[0] + "</b>: " + deadlineFormat(timeObj.datapoint[1]);

        timePlot.highlight(timeObj.series, timeObj.datapoint);
    }

    timeInfo.html(infos);
    lastTimeInfo = timeObj;
}

function showDeadlineDistributionInfo(deadlineDistObj) {
    var infos = "---";

    deadlineDistributionPlot.unhighlight();

    if (deadlineDistObj) {
        var barMax = deadlineFormat(Number(deadlineDistObj.datapoint[0]) +
            Number(deadlineDistributionBarWidth)/0.99);
        var infos = "<b>" + deadlineFormat(deadlineDistObj.datapoint[0]) + " - " +
            barMax.toString() + "</b>: " + deadlineDistObj.datapoint[1] + " Deadlines";

        deadlineDistributionPlot.highlight(deadlineDistObj.series, deadlineDistObj.datapoint);
    }

    deadlineDistributionInfo.html(infos);
    lastDeadlineDistributionInfo = deadlineDistObj;
}

function showDifficultyInfo(difficultyObj) {
    var infos = "---";

    difficultyPlot.unhighlight();

    if (difficultyObj) {
        var infos = "block <b>" + difficultyObj.datapoint[0] + "</b>: " + Math.floor(difficultyObj.datapoint[1]);

        difficultyPlot.highlight(difficultyObj.series, difficultyObj.datapoint);
    }

    difficultyInfo.html(infos);
    lastDifficultyInfo = difficultyObj;
}

function initBlock() {
    system = $("#system");
    bestDeadlineOverallElement = $("#bestOverall");
    bestHistorical = $("#bestHistorical");
    minedBlocksElement = $("#minedBlocks");
    roundsSubmitted = $("#roundsSubmitted");
    hideSameNonces = $("#cbHideSameNonces");
    noncesFound = $("#cbNoncesFound");
    noncesSent = $("#cbNoncesSent");
    noncesConfirmed = $("#cbNoncesConfirmed");
    progressBar = $("#progressBar");
    progressBarVerify = $("#progressBarVerify");
    lastWinnerContainer = $("#lastWinnerContainer");
    lastWinner = $("#lastWinner");
    iconConfirmationSound = $("#iconConfirmationSound");
    avgDeadline = $("#avgDeadline");
    deadlinePerformance = $("#deadlinePerformance");
    connectionStatus = $("#connectionStatus");
    wonBlocks = $("#wonBlocks");
    lowestDiff = $("#lowestDiff");
    highestDiff = $("#highestDiff");
    meanDiff = $("#meanDiff");
    maxRoundTime = $("#maxRoundTime");
    avgRoundTime = $("#avgRoundTime");
    avgBlockTime = $("#avgBlockTime");
    bestDeadlinesChart = $("#deadlinesChart");
    deadlinesInfo = $("#deadlinesInfo");
    timeChart = $("#timeChart");
    timeInfo = $("#timeInfo");
    deadlineDistributionChart = $("#deadlineDistributionChart");
    deadlineDistributionInfo = $("#deadlineDistributionInfo");
    difficultyChart = $("#difficultyChart");
    difficultyInfo = $("#difficultyInfo");
    settingsDlComboboxes = $("#settingsDlComboboxes");

    // ******************************************
    localInitCheckBoxes();
    // ******************************************

    initDeadlinePlot();
    initTimePlot();
    initDeadlineDistributionPlot();
    initDifficultyPlot();
    bestDeadlineOverallElement.html(nullDeadline);
    bestHistorical.html(nullDeadline);
    connectBlock();
    // deActivateConfirmationSound(true);

    // ******************************************
    deActivateConfirmationSound(playConfirmationSound);
    // ******************************************

    showDeadlinesInfo(null);
    showDeadlineDistributionInfo(null);
    showDifficultyInfo(null);
    $("#container").append(miningData.panel);

    hideSameNonces.change(reparseMessages);
    noncesFound.change(reparseMessages);
    noncesSent.change(reparseMessages);
    noncesConfirmed.change(reparseMessages);

    logSettings = initSettings(settingsDlComboboxes, reparseMessages);

    // ******************************************
    localInitLogSettings();
    // ******************************************
}

function initDeadlinePlot() {
    var options = {
        series: {
            points: { show: true, radius:2 }
        },
        grid: {
            hoverable: true,
            autoHighlight: true,
            clickable: true
        },
        xaxis: {
            show: false,
        },
        yaxis: {
            mode: "time",
            min: 0,
            max: deadlinePlotMax,
            tickFormatter: function (val, axis) {
                return deadlineFormatPlot(val);
            }
        },
        colors: ["#2172A2", "#0022FF"]
    };

    deadlinePlot = $.plot(bestDeadlinesChart, [], options);

    bestDeadlinesChart.bind("plotclick", function (event, pos, item) {
        if (item)
            showDeadlinesInfo(item);
    });

    bestDeadlinesChart.bind("plothover", function (event, pos, item) {
        if (item)
            showDeadlinesInfo(item);
    });
}

function initDeadlineDistributionPlot() {
    var options = {
        series: {
            bars: { show: true }
        },
        bars: {
            barWidth: deadlineDistributionBarWidth
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
            min: 0
        },
        colors: ["#2172A2", "#0022FF"]
    };

    deadlineDistributionPlot = $.plot(deadlineDistributionChart, [], options);

    deadlineDistributionChart.bind("plotclick", function (event, pos, item) {
        if (item)
            showDeadlineDistributionInfo(item);
    });

    deadlineDistributionChart.bind("plothover", function (event, pos, item) {
        if (item)
            showDeadlineDistributionInfo(item);
    });
}

function initTimePlot() {
    var options = {
        series: {
            lines: {
                show: true,
                fill: true
            },
            points: {
                show: false,
                radius:2
            }
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
            min: 0,
            max: timePlotMax
        },
        legend: {
            position:"nw",
            backgroundOpacity: 0,
            noColumns: 2
        },
        colors: ["#2172A2", "#FE9E28"]
    };

    timePlot = $.plot(timeChart, [], options);

    timeChart.bind("plotclick", function (event, pos, item) {
        if (item)
            showTimeInfo(item);
    });

    timeChart.bind("plothover", function (event, pos, item) {
        if (item)
            showTimeInfo(item);
    });
}

function initDifficultyPlot() {
    var options = {
        series: {
            lines: {
                show: true,
                fill: true
            },
            points: {
                show: false,
                radius:2
            }
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
            min: 0,
            tickFormatter: function (val, axis) {
                if (val < 1000) return val;
                if (val < 1000000) return val/1000 + "K";
                if (val < 1000000000) return val/1000000 + "M";
            }
        },
        colors: ["#2172A2", "#0022FF"]
    };

    difficultyPlot = $.plot(difficultyChart, [], options);

    difficultyChart.bind("plotclick", function (event, pos, item) {
        if (item)
            showDifficultyInfo(item);
    });

    difficultyChart.bind("plothover", function (event, pos, item) {
        if (item)
            showDifficultyInfo(item);
    });
}

window.onresize = function (evt) {
    if (deadlinePlot) {
        deadlinePlot.resize(0, 0);
        deadlinePlot.setupGrid();
        deadlinePlot.draw();
    }
    if (timePlot) {
        timePlot.resize(0, 0);
        timePlot.setupGrid();
        timePlot.draw();
    }
    if (deadlineDistributionPlot) {
        deadlineDistributionPlot.resize(0, 0);
        deadlineDistributionPlot.setupGrid();
        deadlineDistributionPlot.draw();
    }
    if (difficultyPlot) {
        difficultyPlot.resize(0, 0);
        difficultyPlot.setupGrid();
        difficultyPlot.draw();
    }
}

window.onload = function (evt) {
    $("#btnBlock").addClass('active');
    initBlock();
}