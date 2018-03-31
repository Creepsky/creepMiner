var onlineVersionNum="";
var runningVersionNum="";

window.onload = function (evt) {
    $("#btnAbout").addClass('active');
    printVersions();
}

function printVersions() {
    //check if running newest version
    var onlineVersionSplit = onlineVersion.split(".");
    var runningVersionSplit = runningVersion.split(".");
    console.log(onlineVersionSplit);
    console.log(runningVersionSplit);
    var runningOld=false;
    if (Number(onlineVersionSplit[0]) > Number(runningVersionSplit[0])) 
        runningOld=true;
    else if (Number(onlineVersionSplit[1]) > Number(runningVersionSplit[1])) 
        runningOld=true;
    else if (Number(onlineVersionSplit[2]) > Number(runningVersionSplit[2])) 
        runningOld=true;
    
    //show accordingly
    if(runningOld)
    {
        $("#runningVer").html("creepMiner " + runningVersion);
        $("#latestVer").html("creepMiner " + onlineVersion);
        $("#versionCardHeader").toggleClass("bg-success",false);
        $("#versionCardHeader").toggleClass("bg-danger",true);
        $("#versionCard").append("<div class='alert alert-danger' role='alert'><strong>Bad!</strong>"+
            " You are running an old version of creepMiner. Make sure to get the latest release from <br>"
            +"<a href='https://github.com/Creepsky/creepMiner/releases'>https://github.com/Creepsky/creepMiner/releases</a></div>");
    }else
    {
        $("#runningVer").html("creepMiner " + runningVersion);
        $("#latestVer").html("creepMiner " + onlineVersion);
        $("#versionCard").append("<div class='alert alert-success' role='alert'><strong>Good!</strong>"+
            " You are running a current version of creepMiner.</div>");             
    }
}

