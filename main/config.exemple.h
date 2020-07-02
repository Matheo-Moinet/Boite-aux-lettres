// WiFi Connection Info :

const bool WPA2_Entreprise_enabled = false;			//Set to false to use normal WiFi, true to use WPA2 Entreprise

const String ssid = "unc-user";			//Enter your WiFi SSID here
const String username = "ctira";		//Only used if WPA2 Entreprise is enabled
const String password = "12345678";			//Enter your WiFi password here


// IFTTT Configuration

const String host = "maker.ifttt.com";
const int httpsPort = 443;

const String trigger = "my_trigger";			//Enter your trigger here
const String key =  "fIozadD4HM75vPxjBp-Z57L9gaGHUOrgC3HUI5sa8nr";			//Enter your Weebhook key here


// Detection Configuration

const int time_between_two_measurements = 100; //The number of miliseconds between two measurement.
const float time_before_weebhook_is_triggered = 15;	//The number of seconds before the weebhook is triggered (after the last letter is detected).
const float time_between_two_letters_are_detected = 1; //The number of seconds before a second letter can be detected after a first letter is detected.
const float distance_change_to_detect = 0.5; //The number of centimeters a letter has to be above the bottom to be detected.



// Changing any of the following values is not advised and may result in a non operable device, we recommend only changing them if you know what you are doing.

const int min_time_before_next_ultrasonic = 1000 ;//The minimum time interval between two ultrasonic measurements.
const int led_timer_max = 500 ; //The number of tick the timmer must reach for a led cycle to be completed.
const int button_timer = 60 ; //The number of secondes the detection is stopped after the button is pressed
