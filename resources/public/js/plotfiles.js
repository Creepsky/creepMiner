var plotDirList = $("#plotDirList");
var plotFileDiv = $("#plotFileDiv");
var plotDirElements = [];

var activePlotDir = null;
var confirmedPlotfiles = [];
var current_selected = null;

var isChecking=false;
var isCheckingAll=false;

window.onload = function (evt) {
    $("#btnPlots").addClass('active');
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
                checkVersion(response["runningVersion"], response["onlineVersion"]);
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
                fillDirs();
                break;
            case "plotcheck-result":
                setPlotIntegrity(response);
                break;
            case "totalPlotcheck-result":
                setTotalPlotIntegrity(response);
                break;
            default:
                break;
        }
    }
}

function createProgressBar(id) {
    var progresStr = '<div class="progress">';
    progresStr += '<div id="pb-' + id + '" class="progress-bar progress-bar-success progress-bar-striped bg-success" role="progressbar" aria-valuenow="100" aria-valuemin="0" aria-valuemax="100" style="width:100%">';
    progresStr += '</div></div>';
    return progresStr;
}

function createDirLine(dirElement, index) {
    var line = $('<a href="#" class="list-group-item"></a>');
    var removeElement = '<span id="removeElement" data-toggle="tooltip" data-placement="bottom" data-original-title="Remove plot directory." onclick="delPlotDir(\'' + dirElement["path"] + '\');">';

    line.append(dirElement["path"]);
    line.append(" (" + dirElement.plotfiles.length + " files, " + dirElement["size"] + ") <small class='float-sm-right'>" + removeElement + "<i class='fas fa-minus text-danger'></i></span></small>");
    line.append(createProgressBar(index));

    line.click(function () {
        plotDirList.find('.active').removeClass('active');
        line.addClass('active');
        showPlotfiles(plotDirElements[index]["plotfiles"]); // Old version: showPlotfiles(dirElement["plotfiles"]);
        activePlotDir = dirElement;
        colorConfirmedPlotfiles();
        current_selected = dirElement;
    });

    return line;
}

function fillDirs() {
    plotDirList.empty();
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
            var lineFile = createPlotfileLine(account, start_nonce, nonces, staggersize, size, path);

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

        element["element"] = createDirLine(element, index);
        element["setProgress"] = function (progress) {
            setProgress($("#pb-" + index), progress);
        };
        element["progressBarType"] = "";
        element["resetProgressBarType"] = function () {
            if (element["progressBarType"].length > 0)
                $("#pb-" + index).removeClass(element["progressBarType"]);
        };
        element["setProgressBarType"] = function (type) {
            element.resetProgressBarType();
            $("#pb-" + index).addClass(type);
            element["progressBarType"] = type;
        }

        plotDirElements.push(element);
    });
}

function checkPlotFile(account, start_nonce, nonces, staggersize, path) {
    if(!isChecking && !isCheckingAll)
    {
        isChecking=true;
        var butId = account + "_" + start_nonce + "_" + nonces + "_" + staggersize;
        $("#"+butId).html("<i class='fas fa-spinner fa-pulse'></i>");
        $.get(encodeURI("/checkPlotFile/" + path));
    }
}

function checkAllPlotFiles(account, start_nonce, nonces, staggersize, path) {
    if(!isCheckingAll)
    {
        isCheckingAll=true;
        $("#CheckAllButton").html("<i class='fas fa-spinner fa-pulse'></i>");
        $.get('/checkPlotFile/all');
    }
}

function setPlotIntegrity(checkPlotResult) {
    isChecking=false;
    var integrity = Math.floor(Number(checkPlotResult["plotIntegrity"])*100)/100;
    //find the corresponding element in the plotDirElements and write the integrity into it.
    plotDirElements.forEach(function (element, indexDir, arrayFold) {
        element["plotfiles"].forEach( function(fileElement, index, array) {
            if(fileElement["element"].find("#"+checkPlotResult["plotID"])[0]){
                if (integrity==100)
                {
                    plotDirElements[indexDir]["plotfiles"][index]["element"].find("#"+checkPlotResult["plotID"]).
                        html("<i class='fas fa-check'></i>&nbsp;&nbsp;" + integrity + "%");
                    plotDirElements[indexDir]["plotfiles"][index]["element"].find("#"+checkPlotResult["plotID"]).
                        toggleClass('btn-info btn-danger',false);
                    plotDirElements[indexDir]["plotfiles"][index]["element"].find("#"+checkPlotResult["plotID"]).
                        toggleClass('btn-success',true);
                }
                else
                {
                    plotDirElements[indexDir]["plotfiles"][index]["element"].find("#"+checkPlotResult["plotID"]).
                        html("<i class='fas fa-exclamation'></i>&nbsp;&nbsp;" + integrity + "%");
                    plotDirElements[indexDir]["plotfiles"][index]["element"].find("#"+checkPlotResult["plotID"]).
                        toggleClass('btn-info btn-success',false);
                    plotDirElements[indexDir]["plotfiles"][index]["element"].find("#"+checkPlotResult["plotID"]).
                        toggleClass('btn-danger',true);
                }
            }
        });
});
}

function setTotalPlotIntegrity(totalCheckResult) {
    isCheckingAll=false;
    var totIntegrity = Math.round(Number(totalCheckResult["totalPlotIntegrity"])*100)/100;
    if (totIntegrity==100)
    {
        $("#CheckAllButton").html("<i class='fas fa-check'></i>&nbsp;&nbsp;" + totIntegrity + "% Integrity");
        $("#CheckAllButton").toggleClass('btn-info btn-danger',false);
        $("#CheckAllButton").toggleClass('btn-success',true);
    }
    else
    {
        $("#CheckAllButton").html("<i class='fas fa-exclamation'></i>&nbsp;&nbsp;" + totIntegrity + "% Integrity");
        $("#CheckAllButton").toggleClass('btn-info btn-success',false);
        $("#CheckAllButton").toggleClass('btn-danger',true);
    }
}

function createPlotfileLine(account, start_nonce, nonces, staggersize, size, path) {
    var line = $("<li class='list-group-item d-md-flex align-items-center' style='padding:4px'></li>");
    line.append("<div class='col-xs-3 col-md-3'>" + account + "</div>");
    line.append("<div class='col-xs-2 col-md-2'>" + start_nonce + "</div>");
    line.append("<div class='col-xs-2 col-md-1'>" + nonces + "</div>");
    line.append("<div class='col-xs-2 col-md-1'>" + staggersize + "</div>");
    line.append("<div class='col-xs-1 col-md-2'>" + size + "</div>");
    line.append("<div class='col-xs-2 col-md-3'><button id='" + account + "_" + start_nonce + "_" + nonces + "_" + staggersize +
        "' type='button' class='btn btn-primary' " +
        "style='padding:2px; margin=0px; width:100%' onclick='checkPlotFile(\"" + account + "\"," + start_nonce + "," +
        nonces + "," + staggersize + ",\"" + path.replace(/\\/g,"\\\\") + "\")'><span class='fab fa-superpowers'></span>&nbsp;Validate</button></div>");
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

$('#removeElement').tooltip();

function delPlotDir(path) {
	swal( {
		title: 'Remove plot directory',
		html: 
        '<p>Are you sure you want to remove this directory?</p>' +
        'If you are sure, type in <b>YES</b>',
		type: 'warning',
        input: "text",
		showCancelButton: true,
		confirmButtonText: 'Delete',
		cancelButtonText: 'Cancel',
        confirmButtonColor: "#dc3545",
        reverseButtons: true,
	}).then((result) => {
        if (result.value.trim().toLowerCase() == 'yes') {
			$.ajax( {
				type: "POST",
                url: "plotdir/remove",
                data: (path),
                success: function(msg){
                    if (!msg.error) {
                        swal({
                            title: "Successfully deleted!",
                            text: "The plot directory has been removed",
                            type: "success"
                        });
                    }
                    else {
                        swal({
                            title: "Error!",
                            text: "There was an error removing the plot directory: " + msg.error,
                            type: "error"
                        });
                    }
                }
    		} );
        }
    });
}

function addPlotDir() {
    swal({
    title: 'Add new directory',
    html:
    '<p>Enter your plot directory</p>' +
    '<div style="font-size:3em; color:Tomato"><i class="fas fa-bug"></i></div><br>' +
    '<b>still need to implement a browse button<b>',
    input: "text",
    showCancelButton: true,
    confirmButtonText: "Add",
    cancelButtonText: "Cancel",
    confirmButtonColor: "#1FAB45",
    buttonsStyling: true
    }).then((result) => {
        if (result.value) {
      	    $.ajax( {
                type: "POST",
                url: "plotdir/add",
                data: (result.value),
                success: function (msg) {
                    if (!msg.error) {
                        swal({
                            title: "Successfully added!",
                            text: "Your plot directory has been added",
                            type: "success"
                        })
                    }
                    else {
                        swal({
                            title: "Error!",
                            html: "<p>In adding the plot directory</p><p>'" + msg.error + "'</p><p><b>Check that it is a valid directory!</b><p>",
                            type: "error"
                        })
                    }
                }
          } );
        }
    });
}
