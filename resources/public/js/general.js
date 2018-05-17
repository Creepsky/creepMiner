var websocket;
var servername = 'creepMiner';
var MasterMiner = 'false';
var loggers = [
	["miner", "Miner", 6],
	["config", "Config", 6],
	["server", "Server", 1],
	["socket", "Socket", 0],
	["session", "Session", 3],
	["nonceSubmitter", "Nonce submitter", 6],
	["plotReader", "Plot reader", 6],
	["plotVerifier", "Plot verifier", 6],
	["wallet", "Wallet", 1],
	["general", "General", 6]
];
var levels = [
	"off", "fatal", "critical", "error", "warning",
	"notice", "information", "debug", "trace", "all"
];

function connect(onMessage) {
	if ("WebSocket" in window) {
		if (websocket)
			websocket.close();

		if (location.protocol == "https:")
			protocol ="wss"
		else
			protocol = "ws"

		websocket = new WebSocket(protocol + "://" + window.location.host);
		websocket.onmessage = onMessage;
	}
	else {
		websocket = null;
	}
}

// version checker and update about btn
function checkVersion(runningVer, onlineVer) {
	var onlineVersionSplit = onlineVer.split(".");
    var runningVersionSplit = runningVer.split(".");
	var current=true;
    if (Number(onlineVersionSplit[0]) > Number(runningVersionSplit[0]))
        current=false;
    else if (Number(onlineVersionSplit[1]) > Number(runningVersionSplit[1]))
        current=false;
    else if (Number(onlineVersionSplit[2]) > Number(runningVersionSplit[2]))
        current=false;
	if(!current)
	{
        $("#btnAbout").find("a").css({"color":"#dc3545"});
        $("#btnAbout").attr({"data-original-title": "A new version is available"});
        $("#runningVer").html("&nbsp;v&nbsp;" + runningVer);
        $("#latestVer").html("&nbsp;v&nbsp;" + onlineVer);
        $("#versionCardHeader").toggleClass("bg-success",false);
        $("#versionCardHeader").toggleClass("bg-danger",true);
        $("#versionAlert").toggleClass("alert-success",false);
        $("#versionAlert").toggleClass("alert-danger",true);
        $("#versionAlert").html("<i class='fas fa-wrench'></i>&nbsp;&nbsp;<strong>Update available!</strong>"+
            " Download the latest from <i class='fab fa-github-square'></i>&nbsp;<a href='https://github.com/Creepsky/creepMiner/releases'>github</a>");
    } else
    {
        $("#runningVer").html("<i class='fas fa-check text-success'></i>&nbsp;v&nbsp;" + runningVer);
        $("#latestVer").html("&nbsp;v&nbsp;" + onlineVer);
        $("#versionAlert").html("<i class='fas fa-check text-success'></i>&nbsp;&nbsp;You are running a current version of creepMiner.");
    }
	return current
}

// progress bar
function setProgress(progressBar, progress) {
	var valueFixed = parseFloat(progress).toFixed();

	if (valueFixed >= 100) {
		valueFixed = 100;
		progressBar.removeClass("active");
	}
	else if (valueFixed < 100) {
		if (valueFixed < 0)
			valueFixed = 0;

		if (!progressBar.hasClass("active"))
			progressBar.addClass("active");
	}

	progressBar.css("width", valueFixed + "%").attr("aria-valuenow", valueFixed);
	progressBar.html("");
}

// verify progress bar
function setProgressVerify(progressBar, progress) {
	var valueFixed = parseFloat(progress).toFixed();

	if (valueFixed >= 100) {
		valueFixed = 100;
		progressBar.removeClass("active");
	}
	else if (valueFixed < 100) {
		if (valueFixed < 0)
			valueFixed = 0;

		if (!progressBar.hasClass("active"))
			progressBar.addClass("active");
	}

	progressBar.css("width", valueFixed + "%").attr("aria-valuenow", valueFixed);
	progressBar.html(valueFixed + " % Verified");
}

// initializing settings
function initSettings(container, onChange) {
	output = {};
	loggers.forEach(function (element, index, array) {
		element[1][0].toUpperCase();
		var cmb = $("<select id='cmb_" + element[0] + "' name='cmb_" + element[0] + "' class='selectpicker form-control'></select>");
		createLoggerCombobox(cmb);
		if (onChange)
			cmb.change(function () {
				onChange();
			});

		var div = $("<div class='form-group row'></div>");
		var label = $("<label for='cmb_" + element[0] + "' class='col-md-12 col-lg-3 col-form-label'>" + element[1] + "</label>");

		div.append(label);
		div.append("<div class='col-md-12 col-lg-9'>")
		div.find("div").append(cmb);

		cmb.val(element[2]);
		output[element[0]] = cmb;

		container.append(div);
	});
	return output;
}

// create combo boxes for logger
function createLoggerCombobox(cmb) {
	cmb.empty();
	levels.forEach(function (element, index, array) {
		cmb.append("<option value=" + index + ">" + element + "</option>");
	});
}

// initializing tooltip
$(document).ready(function() {
  $('[data-toggle="tooltip"]').tooltip();
});

// stylesheet selector
(function($)
 {
  var $links = $('link[rel*=alternate][title]');
  var el = document.getElementById('themeSelector');
  var options= '<a class="dropdown-item" onclick="eraseCookie(\'theme\');" style="cursor:pointer">Default</a><div class="dropdown-divider"></div>';
  $links.each(function(index,value){
   options +='<a class="dropdown-item" onclick="SwitchTheme(\''+$(this).attr('title')+'\'); location.reload();" style="cursor:pointer">'+$(this).attr('title')+'</a>';
  });
  $links.remove();

  el.innerHTML = options;
 }
)(jQuery);

// dynamically switch bootswatch themes
function SwitchTheme (name){
 $('link[rel*=jquery]').remove();
 console.log(name);
 $('head').append('<link rel="stylesheet jquery" href="https://bootswatch.com/4/'+ name +'/bootstrap.min.css" type="text/css" />');
 document.cookie = "theme = https://bootswatch.com/4/"+name+"/bootstrap.min.css;";
}

// fetch cookie
function getCookie(cname) {
    var name = cname + "=";
    var decodedCookie = decodeURIComponent(document.cookie);
    var ca = decodedCookie.split(';');
    for(var i = 0; i <ca.length; i++) {
        var c = ca[i];
        while (c.charAt(0) == ' ') {
            c = c.substring(1);
        }
        if (c.indexOf(name) == 0) {
            return c.substring(name.length, c.length);
        }
    }
    return "";
}

// to add new cookies, use this to create future cookies - will be used later on
function createCookie(name,value,days) {
	if (days) {
		var date = new Date();
		date.setTime(date.getTime()+(days*24*60*60*1000));
		var expires = "; expires="+date.toGMTString();
	}
	else var expires = "";
	document.cookie = name+"="+value+expires+"; path=/";
}

// remove cookie
function eraseCookie(name) {
	window.location.reload(false);
	createCookie(name,"",-1);
}

// set the title in header and replace the server-name element with server name
document.getElementById('server-name').innerHTML = servername;
document.title = servername;

// if master miner is set show and hide some elements
if (MasterMiner == 'true') {
	var mmElement = document.getElementsByClassName('mm-show'), i;
	for (var i = 0; i < mmElement.length; i ++) {
	    mmElement[i].style.display = 'inline';
	}
	var mmHideElements = document.getElementsByClassName('mm-hide'), i;
	for (var i = 0; i < mmHideElements.length; i ++) {
	    mmHideElements[i].style.display = 'none';
	}
}

// master miner menu - WIP
$.getJSON('https://next.json-generator.com/api/json/get/VJs_LJEAN', function(data) {
    var builddata = function() {
        var source = [];
        var items = [];
        for (i = 0; i < data.length; i++) {
            var item = data[i];
            var label = item["name"];
            var parentid = item["parent_id"];
            var icon = item["icon"];
            var id = item["id"];
            var url = item["url"];

            if (items[parentid]) {
                var item = {
                    parentid: parentid,
                    label: label,
                    url: url,
                    icon: icon,
                    item: item
                };
                if (!items[parentid].items) {
                    items[parentid].items = [];
                }
                items[parentid].items[items[parentid].items.length] = item;
                items[id] = item;
            } else {
                items[id] = {
                    parentid: parentid,
                    label: label,
                    url: url,
                    icon: icon,
                    item: item
                };
                source[id] = items[id];
            }
        }
        return source;
    }
    // build dynamic menu from json payload
    var buildMenu = function(menu, source) {
        var isFirst = true;
        $.each(source, function() {
            if (this.label) {
                if (!isFirst) {
                    var divider = $('<div class="dropdown-divider"></div>');
                    menu.append(divider);
                }
                var parent = $('<a class="dropdown-item" href="' + this.url + '" target="_blank"><i class="text-muted ' + this.icon + '"></i>  ' + this.label + '</a>');
                menu.append(parent);
                if (this.items && this.items.length > 0) {
                    $.each(this.items, function() {
                        var child = $('<a class="dropdown-item" href="' + this.url + '" target="_blank"><i class="text-muted ' + this.icon + '" ></i>  ' + this.label + '</a>');
                        menu.append(child);
                    });
                }
                isFirst = false;
            }
        });
    }
    var source = builddata();
    console.log(source);
    buildMenu($('#dynamic-menu'), source);
});
