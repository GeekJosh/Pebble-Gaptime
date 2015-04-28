const APP_VERSION = "1.4";

var sunTimes;

var locationOptions = {
    enableHighAccuracy: false,
    maximumAge: 10000,
    timeout: 10000
}

function locationSuccess(pos) {
    console.log("lat: " + pos.coords.latitude + " lon: " + pos.coords.longitude);
    sunTimes = SunCalc.getTimes(new Date(), pos.coords.latitude, pos.coords.longitude);
    console.log("Sunrise: " + sunTimes.sunrise.getHours() + ":" + sunTimes.sunrise.getMinutes());
    console.log("Sunset: " + sunTimes.sunset.getHours() + ":" + sunTimes.sunset.getMinutes());
}

function locationError(err) {
    console.log("Location error (" + err.code + "): " + err, message);
}

function updateLocation() {
    navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
}

Pebble.addEventListener("ready",
						function (e) {
						    console.log("PebbleKit JS ready!");
						    updateLocation();
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

						    //Send to Pebble, persist there
						    Pebble.sendAppMessage(
								{
								    "KEY_TEXT_TIME": configuration.textTime,
								    "KEY_HAND_ORDER": configuration.handOrder,
								    "KEY_INVERT": configuration.invert,
								    "KEY_INVERT_START": configuration.invertStart,
								    "KEY_INVERT_END": configuration.invertEnd
								},
								function (e) {
								    console.log("Sending settings data...");
								},
								function (e) {
								    console.log("Settings feedback failed!");
								});
						});

Pebble.addEventListener("appmessage",
    function (e) {
        console.log(JSON.stringify(e.payload));
    });
