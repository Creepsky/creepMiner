var logSetting = {};

function update_settings(config) {
    mining_info_url.val(config['miningInfoUrl'] + ':' + config['miningInfoUrlPort']);
    submission_url.val(config['poolUrl'] + ':' + config['poolUrlPort']);
	if (config['walletUrl'] + ':' + config['walletUrlPort']=="://:0")
		wallet_url.val("");
	else
		wallet_url.val(config['walletUrl'] + ':' + config['walletUrlPort']);

    intensity.val(config['miningIntensityRaw']);
    buffer_size.val(config['bufferSizeRaw']);
    buffer_chunks.val(config['bufferChunks']);
    plot_readers.val(config['maxPlotReadersRaw']);
    submission_max_retry.val(config['submissionMaxRetry']);
    submit_probability.val(config['submitProbability']);
    max_historical_blocks.val(config['maxHistoricalBlocks']);
    target_deadline.val(config['targetDeadlineLocal']);

    timeout.val(config['timeout']);

    log_dir.val(config['logDir']);

    for (var key in config['channelPriorities']) {
        window['cmb_' + key].val(config['channelPriorities'][key].numeric);
    }
}

function connectCallback(msg) {
    data = msg["data"];

    if (data) {
        if (data == "ping") {
            return;
        }

        var response = JSON.parse(data);

        switch (response["type"]) {
            case "config":
                update_settings(response);
                break;
            case "new block":
                checkVersion(response["runningVersion"], response["onlineVersion"], response["runningBuild"]);
                break;
            default:
                break;
        }
    }
}

window.onload = function (evt) {
    $("#btnSettings").addClass('active');
    logSetting = initSettings($("#settingsDlComboboxes"));

    mining_info_url = $("#mining-info-url");
    submission_url = $("#submission-url");
    wallet_url = $("#wallet-url");

    intensity = $("#intensity");
    buffer_size = $("#buffer-size");
    buffer_chunks = $("#buffer-chunks");
    plot_readers = $("#plot-readers");
    submission_max_retry = $("#submission-max-retry");
    submit_probability = $("#submit-probability");
    max_historical_blocks = $("#max-historical-blocks");
    target_deadline = $("#target-deadline");
    timeout = $("#timeout");

    log_dir = $("#log-dir");

    loggers.forEach(function (element) {
        var id = "cmb_" + element[0];
        window[id] = $("#" + id);
    });

    connect(connectCallback);
}

function restartMiner() {
	swal( {
		title: 'Restart creepMiner',
		html:
        '<p>Are you 100% sure you want to restart?</p>' +
        'If you are sure, type in <b>restart</b>',
		type: 'warning',
        input: "text",
		showCancelButton: true,
		confirmButtonText: 'Restart',
		cancelButtonText: 'Cancel',
        confirmButtonColor: "#dc3545",
        reverseButtons: true,
	}).then((result) => {
        if (result.value.trim().toLowerCase() == 'restart') {
				$.ajax( {
					type: "GET",
	                url: "/restart",
	                success: function(msg){
	                    if (!msg.error) {
	                        swal({
	                            title: "Successfully queued!",
	                            html: "<p><b>creepMiner</b> has been restarted,</p> <p>please wait for it to startup.</p>",
	                            type: "success"
	                        });
	                    }
	                    else {
	                        swal({
	                            title: "Error!",
	                            html: "There was an error removing the plot directory: " + msg.error,
	                            type: "error"
	                        });
	                    }
	                }
	    		} );
        }
        else {
					swal({
						title: "Error!",
						html: "<p>In adding the plot directory</p><p>'" + msg.error + "'</p><p><b>Check that it is a valid directory!</b><p>",
						type: "error"
					})
				}
    });
}
