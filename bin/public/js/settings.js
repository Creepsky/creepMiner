var logSetting = [];

function update_settings(config){
    mining_info_url.val(config['miningInfoUrl'] + ':' + config['miningInfoUrlPort']);
    submission_url.val(config['poolUrl'] + ':' + config['poolUrlPort']);
    wallet_url.val(config['walletUrl'] + ':' + config['walletUrlPort']);

    intensity.val(config['miningIntensity']);
    buffer_size.val(config['bufferSizeMB']);
    plot_readers.val(config['maxPlotReaders']);
    submission_max_retry.val(config['submissionMaxRetry']);

    var tdo = config['targetDeadlineObject'];
    target_deadline_years.val(tdo.years);
    target_deadline_months.val(tdo.months);
    target_deadline_days.val(tdo.days);
    target_deadline_hours.val(tdo.hours);
    target_deadline_minutes.val(tdo.minutes);
    target_deadline_seconds.val(tdo.seconds);

    timeout.val(config['timeout']);
}

function connectCallback(msg){
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
            default:
                break;
        }
    }
}

window.onload = function(evt) {
	logSetting = initSettings($("#settingsDlComboboxes"));

    mining_info_url = $("#mining-info-url");
    submission_url = $("#submission-url");
    wallet_url = $("#wallet-url");

    intensity = $("#intensity");
    buffer_size = $("#buffer-size");
    plot_readers = $("#plot-readers");
    submission_max_retry = $("#submission-max-retry");
    target_deadline_years = $("#target-deadline-years");
    target_deadline_months = $("#target-deadline-months");
    target_deadline_days = $("#target-deadline-days");
    target_deadline_hours = $("#target-deadline-hours");
    target_deadline_minutes = $("#target-deadline-minutes");
    target_deadline_seconds = $("#target-deadline-seconds");
    timeout = $("#timeout");

    connect(connectCallback);
}