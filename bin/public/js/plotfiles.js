var plotDirList = $("#plotDirList");
var plotFileDiv = $("#plotFileDiv");
var plotDirElements = [];

var activePlotDir = null;
var confirmedPlotfiles = [];
var current_selected = null;

window.onload = function (evt) {
    $("#btnPlotfiles").addClass('active');
    parsePlots();
    fillDirs();
    connect(connectCallback);
}

function connectCallback(msg) {
    data = msg["data"];

    if (data) {
        if (data == "ping") {
            return;
        }

        var response = JSON.parse(data);

        switch (response["type"]) {
            case "new block":
                resetProgress();
                resetLineTypes();
                confirmedPlotfiles = [];
                break;
            case "plotdir-progress":
                setDirProgress(response["dir"], response["value"]);
                break;
            case "nonce confirmed":
                nonceConfirmed(response["plotfile"]);
                break;
            case "plotdirs-rescan":
                plotdirs = response["plotdirs"];
                parsePlots();
                break;
            default:
                break;
        }
    }
}

function createProgressBar() {
    var progresStr = '<div class="progress">';
    progresStr += '<div class="progress-bar progress-bar-striped" role="progressbar" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100" style="width:0%">';
    progresStr += '</div></div>';
    return progresStr;
}

function createDirLine(dirElement) {
    //var line = $('<li class="list-group-item"></li>');
    var line = $('<a href="#" class="list-group-item"></a>');

    line.append(dirElement["path"]);
    line.append(" (" + dirElement.plotfiles.length + " files, " + dirElement["size"] + ")");
    line.append(createProgressBar());

    line.click(function () {
        showPlotfiles(dirElement["plotfiles"]);
        activePlotDir = dirElement;
        colorConfirmedPlotfiles();
        current_selected = dirElement;
    });

    return line;
}

function fillDirs() {
    plotDirElements.forEach(function (plotDirElement, index, array) {
        plotDirList.append(plotDirElement["element"]);
    });
}

function parsePlots() {
    plotDirElements = [];

    plotdirs.forEach(function (plotDir, index, array) {
        var element = {
            "path": plotDir["path"],
            "size": plotDir["size"],
            "plotfiles": []
        };

        plotDir["plotfiles"].forEach(function (plotFile, index, array) {
            var path = plotFile["path"];
            var filename = path.replace(/^.*[\\\/]/, '');
            var filename_tokens = filename.split("_");
            var account = filename_tokens[0];
            var start_nonce = filename_tokens[1];
            var nonces = filename_tokens[2];
            var staggersize = filename_tokens[3];
            var size = plotFile["size"];
            var lineFile = createPlotfileLine(account, start_nonce, nonces, staggersize, size);

            var fileElement = {
                "path": path,
                "size": size,
                "element": lineFile,
                "lineType": null,
                "setLineType": function (type) {
                    fileElement.element.addClass(type);
                    fileElement.lineType = type;
                },
                "resetLineType": function () {
                    if (fileElement.lineType) {
                        fileElement.element.removeClass(fileElement.lineType);
                        fileElement.lineType = null;
                    }
                }
            };

            element["plotfiles"].push(fileElement);
        });

        element["element"] = createDirLine(element);
        element["progressBar"] = element["element"].find('.progress .progress-bar');
        element["setProgress"] = function (progress) {
            setProgress(element["progressBar"], progress);
        };
        element["progressBarType"] = "";
        element["resetProgressBarType"] = function () {
            if (element["progressBarType"].length > 0)
                element.progressBar.removeClass(element["progressBarType"]);
        };
        element["setProgressBarType"] = function (type) {
            element.resetProgressBarType();
            element.progressBar.addClass(type);
            element["progressBarType"] = type;
        }

        plotDirElements.push(element);
    });
}

function createPlotfileLine(account, start_nonce, nonces, staggersize, size) {
    var line = $("<tr></tr>");
    line.append("<td>" + account + "</td>");
    line.append("<td>" + start_nonce + "</td>");
    line.append("<td>" + nonces + "</td>");
    line.append("<td>" + staggersize + "</td>");
    line.append("<td>" + size + "</td>");

    return line;
}

function showPlotfiles(files) {
    plotFileDiv.hide('fast', function () {
        plotFileDiv.empty();

        files.forEach(function (file, i, arr) {
            plotFileDiv.append(file["element"]);
        });

        plotFileDiv.show('slow');
    });
}

function resetProgress() {
    plotDirElements.forEach(function (element, index, array) {
        element.setProgress(0);
        element.resetProgressBarType();
    });
}

function setDirProgress(dir, progress) {
    for (var i = 0; i < plotDirElements.length; ++i) {
        if (plotDirElements[i]["path"] == dir) {
            plotDirElements[i].setProgress(progress);
            break;
        }
    }
}

function resetLineTypes() {
    if (activePlotDir) {
        for (var i = 0; i < activePlotDir.plotfiles.length; ++i) {
            activePlotDir.plotfiles[i].resetLineType();
        }
    }
}

function colorConfirmedPlotfiles() {
    if (!activePlotDir) {
        return;
    }

    for (var j = 0; j < confirmedPlotfiles.length; ++j) {
        for (var i = 0; i < activePlotDir.plotfiles.length; ++i) {
            if (activePlotDir.plotfiles[i].path == confirmedPlotfiles[j]) {
                activePlotDir.plotfiles[i].setLineType('success');
                break;
            }
        }
    }
}

function nonceConfirmed(plotfile) {
    var dir = null;

    for (var i = 0; i < plotDirElements.length; ++i) {
        if (plotfile.length > plotDirElements[i].path.length) {
            if (plotfile.substring(0, plotDirElements[i].path.length) == plotDirElements[i].path) {
                plotDirElements[i].setProgressBarType('progress-bar-success');
                dir = plotfile.substring(0, plotDirElements[i].path.length);
                break;
            }
        }
    }

    confirmedPlotfiles.push(plotfile);
    colorConfirmedPlotfiles();
}

function remove_selected_plot_dir() {
    if (current_selected) {
        $.get("/");
    }
}
