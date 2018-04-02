window.onload = function (evt) {
    $("#btnAbout").addClass('active');
    printVersions();
}

function printVersions() {
    //check if running newest version
    var currentVersion = checkVersion(runningVersion, onlineVersion);
    //show accordingly
    if(currentVersion)
    {
        $("#runningVer").html("<i class='fa fa-check text-success'></i>&nbsp;v&nbsp;" + runningVersion);
        $("#latestVer").html("&nbsp;v&nbsp;" + onlineVersion);
    }else
    {
        $("#runningVer").html("&nbsp;v&nbsp;" + runningVersion);
        $("#latestVer").html("&nbsp;v&nbsp;" + onlineVersion);
        $("#versionCardHeader").toggleClass("bg-success",false);
        $("#versionCardHeader").toggleClass("bg-danger",true);
        $("#versionCard").append("<div class='alert alert-danger' role='alert'><i class='fa fa-exclamation-triangle text-warning'></i>&nbsp;&nbsp;<strong>Out of date!</strong>"+
					" Download the latest from <a href='https://github.com/Creepsky/creepMiner/releases'> github</a></div>");
    }
}

