var APP_VERSION = "1.4";

var sunTimes;
var invert = "off";

var locationOptions = {
    enableHighAccuracy: false,
    maximumAge: 10000,
    timeout: 10000
};

function locationSuccess(pos) {
    console.log("lat: " + pos.coords.latitude + " lon: " + pos.coords.longitude);
    sunTimes = SunCalc.getTimes(new Date(), pos.coords.latitude, pos.coords.longitude);
    console.log("Sunrise: " + sunTimes.sunrise.getHours() + ":" + sunTimes.sunrise.getMinutes());
    console.log("Sunset: " + sunTimes.sunset.getHours() + ":" + sunTimes.sunset.getMinutes());
	
	Pebble.sendAppMessage(
		{
			"KEY_UPDATE_SUNTIMES": 1,
			"KEY_INVERT_START_MIN": (invert == "sunrise" ? sunTimes.sunrise.getMinutes() : sunTimes.sunset.getMinutes()),
			"KEY_INVERT_START_HOUR": (invert == "sunrise" ? sunTimes.sunrise.getHours() : sunTimes.sunset.getHours()),
			"KEY_INVERT_END_MIN": (invert == "sunrise" ? sunTimes.sunset.getMinutes() : sunTimes.sunrise.getMinutes()),
			"KEY_INVERT_END_HOUR": (invert == "sunrise" ? sunTimes.sunset.getHours() : sunTimes.sunrise.getHours())
		},
		function (e) {
			console.log("Sunrise data received by Pebble");
		},
		function (e) {
			console.warn("Sunrise data not acknowledged by Pebble");
		}
	);
}

function locationError(err) {
    console.error("Location error (" + err.code + "): " + err.message);
}

function updateLocation() {
    navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
}

Pebble.addEventListener("ready",
						function (e) {
							console.log("PebbleKit JS ready!");
						});

Pebble.addEventListener("showConfiguration",
						function (e) {
							//Load the remote config page
							Pebble.openURL("http://geekjosh.github.io/Pebble-Gaptime/appconfig.html?version=" + APP_VERSION);
						});

Pebble.addEventListener("webviewclosed",
						function (e) {
							//Get JSON dictionary
							var configuration = JSON.parse(decodeURIComponent(e.response));
							console.log("Configuration window returned: " + JSON.stringify(configuration));
							invert = configuration.invert;
							
							//Send to Pebble, persist there
							Pebble.sendAppMessage(
								{
									"KEY_TEXT_TIME": configuration.textTime,
									"KEY_HAND_ORDER": configuration.handOrder,
									"KEY_INVERT": configuration.invert,
									"KEY_INVERT_START_MIN": configuration.invertStartMin,
									"KEY_INVERT_START_HOUR": configuration.invertStartHour,
									"KEY_INVERT_END_MIN": configuration.invertEndMin,
									"KEY_INVERT_END_HOUR": configuration.invertEndHour
								},
								function (e) {
									console.log("Settings received by Pebble");
								},
								function (e) {
									console.warn("Settings not acknowledged by Pebble");
								});
						});

Pebble.addEventListener("appmessage",
    function (e) {
		console.log("appmessage :" + JSON.stringify(e.payload));
		if("KEY_UPDATE_SUNTIMES" in e.payload) {
			updateLocation();
		}
    });
