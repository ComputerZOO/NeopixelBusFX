//#######################################################################################################
//#################################### Plugin 124: NeoPixelBusFX ########################################
//#######################################################################################################
/*
List of commands:

nfx off [fadetime] [delay]
switches the stripe off

nfx on [fadetime] [delay]
restores last state of the stripe

nfx dim [dimvalue]
dimvalue 0-255

nfx line startpixel endpixel color

nfx one pixel color

nfx all color [fadetime] [delay]
nfx rgb color [fadetime] [delay]
nfx fade color [fadetime] [delay]

nfx colorfade startcolor endcolor [startpixel] [endpixel]

nfx rainbow [speed]

nfx kitt color [speed]

nfx comet color [speed]

nfx theatre color [backgroundcolor] [count] [speed]

nfx scan color [backgroundcolor] [speed]

nfx dualscan color [backgroundcolor] [speed]

nfx twinkle color [backgroundcolor] [speed]

nfx twinklefade color [count] [speed]

nfx sparkle color [backgroundcolor] [speed]

nfx wipe color [dotcolor] [speed]

nfx dualwipe [dotcolor] [speed]

nfx fire [fps] [brightness] [cooling] [sparking]

nfx faketv [startpixel] [endpixel]

nfx stop
	stops the effect

nfx statusrequest
	sends status

nfx fadetime
nfx fadedelay
nfx speed
nfx count
nfx bgcolor
	sets default parameter

Use:

needed:
color,backgroundcolor -> targetcolor in hex format e.g. ff0000 for red

[optional]:
fadetime ->  fadetime per pixel in ms
delay ->  delay time to next pixel in ms, if delay < 0 fades from other end of the stripe
speed -> 0-50, speed < 0 for reverse


Based on Adafruit Fake TV Light for Engineers, WS2812FX, NeoPixelBus, Lights, NeoPixel - Basic and Candle modules

https://learn.adafruit.com/fake-tv-light-for-engineers/overview
https://github.com/letscontrolit/ESPEasy
https://github.com/kitesurfer1404/WS2812FX
https://github.com/Makuna/NeoPixelBus
https://github.com/ddtlabs/ESPEasy-Plugin-Lights

Thank you to all developers
*/

#include <NeoPixelBrightnessBus.h>
#include <FastLED.h> //for math operations in FireFX
#include <faketv.h> //color pattern for FakeTV

#define SPEED_MAX 50
#define ARRAYSIZE 300 //Max LED Count
#define NEOPIXEL_LIB NeoPixelBrightnessBus // Neopixel library type
#define FEATURE NeoGrbFeature	//NeoBrgFeature //Color order
#define METHOD NeoEsp8266Uart800KbpsMethod //GPIO2 - use NeoEsp8266Dma800KbpsMethod for GPIO3(TX)

#define  numPixels (sizeof(ftv_colors) / sizeof(ftv_colors[0]))

NEOPIXEL_LIB<FEATURE, METHOD> *Plugin_124_pixels;

const float pi = 3.1415926535897932384626433832795;

uint8_t pos,
color,
r_pixel,
startpixel,
endpixel,
difference,
colorcount;

uint16_t ftv_pr = 0, ftv_pg = 0, ftv_pb = 0; // Prev R, G, B;

RgbColor rgb_target[ARRAYSIZE],
rgb_old[ARRAYSIZE],
rgb, rrggbb;

int16_t pixelCount = ARRAYSIZE,
fadedelay = 20;

int8_t	defaultspeed = 25,
rainbowspeed = 1,
speed = 25,
fps = 50,
count = 1;

uint32_t	_counter_mode_step = 0,
fadetime = 1000,
ftv_holdTime,
pixelNum;

String colorStr,
backgroundcolorStr;

bool gReverseDirection = false;

byte cooling = 50,
sparking = 120,
brightness = 31;

unsigned long counter20ms = 0,
starttime[ARRAYSIZE],
maxtime = 0;

enum modetype {
	Off, On, Fade, ColorFade, Rainbow, Kitt, Comet, Theatre, Scan, Dualscan, Twinkle, TwinkleFade, Sparkle, Fire, Wipe, Dualwipe, FakeTV
};

const char* modeName[] = {
	"off", "on", "fade", "colorfade", "rainbow", "kitt", "comet", "theatre", "scan", "dualscan", "twinkle", "twinklefade", "sparkle", "fire", "wipe", "dualwipe", "faketv"
};

modetype mode,savemode,lastmode;

#define PLUGIN_124
#define PLUGIN_ID_124         124
#define PLUGIN_NAME_124       "NeoPixelBusFX"
#define PLUGIN_VALUENAME1_124 "Mode"
#define PLUGIN_VALUENAME2_124 "Lastmode"
#define PLUGIN_VALUENAME3_124 "Fadetime"
#define PLUGIN_VALUENAME4_124 "Fadedelay"

boolean Plugin_124(byte function, struct EventStruct *event, String& string)
{
	boolean success = false;

	switch (function)
	{

		case PLUGIN_DEVICE_ADD:
		{
			Device[++deviceCount].Number = PLUGIN_ID_124;
			Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
			Device[deviceCount].VType = SENSOR_TYPE_QUAD;
			Device[deviceCount].Custom = true;
			Device[deviceCount].Ports = 0;
			Device[deviceCount].PullUpOption = false;
			Device[deviceCount].InverseLogicOption = false;
			Device[deviceCount].FormulaOption = false;
			Device[deviceCount].ValueCount = 4;
			Device[deviceCount].SendDataOption = true;
			Device[deviceCount].TimerOption = true;
			Device[deviceCount].TimerOptional = true;
			Device[deviceCount].GlobalSyncOption = true;
			Device[deviceCount].DecimalsOnly = false;
			break;
		}

		case PLUGIN_GET_DEVICENAME:
		{
			string = F(PLUGIN_NAME_124);
			break;
		}

		case PLUGIN_GET_DEVICEVALUENAMES:
		{
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_124));
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_124));
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_124));
			strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_124));
			break;
		}

		case PLUGIN_WEBFORM_LOAD:
		{
			addFormNumericBox(string, F("Led Count"), F("plugin_124_leds"), Settings.TaskDevicePluginConfig[event->TaskIndex][0],1 ,999);
			string += F("<br><br><span style=\"color:red\">Please connect stripe to GPIO2!</span>");

			success = true;
			break;
		}

		case PLUGIN_WEBFORM_SAVE:
		{
			Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_124_leds"));
			//Settings.TaskDevicePin1[event->TaskIndex] = getFormItemInt(F("taskdevicepin1"));

			success = true;
			break;
		}

		case PLUGIN_INIT:
		{
			if (!Plugin_124_pixels)
			{
				Plugin_124_pixels = new NEOPIXEL_LIB<FEATURE, METHOD>(Settings.TaskDevicePluginConfig[event->TaskIndex][0]);
				Plugin_124_pixels->Begin(); // This initializes the NeoPixelBus library.
			}

			pixelCount = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
			success = true;
			break;
		}

		case PLUGIN_READ:                 // ------------------------------------------->
			{
				// there is no need to read them, just use current values
				UserVar[event->BaseVarIndex] = mode;
				UserVar[event->BaseVarIndex + 1] = savemode;
				UserVar[event->BaseVarIndex + 2] = fadetime;
				UserVar[event->BaseVarIndex + 3] = fadedelay;
				String log;
				log = F("Lights: mode: ");
				log += modeName[mode];
				log += F(" lastmode: ");
				log += modeName[savemode];
				log += F(" fadetime: ");
				log += (int)UserVar[event->BaseVarIndex + 2];
				log += F(" fadedelay: ");
				log += (int)UserVar[event->BaseVarIndex + 3];
				addLog(LOG_LEVEL_INFO, log);

				success = true;
				break;
			}

		case PLUGIN_WRITE:
		{

			String log = "";
			String command = parseString(string, 1);

			if (command == F("neopixelfx") || command == F("nfx")) {
				success = true;
				String subCommand = parseString(string, 2);

				if (subCommand == F("fadetime")) {
					fadetime = parseString(string, 3).toInt();
				}

				else if (subCommand == F("fadedelay")) {
					fadedelay = parseString(string, 3).toInt();
				}

				else if (subCommand == F("speed")) {
					defaultspeed = parseString(string, 3).toInt();
					speed = defaultspeed;
				}

				else if (subCommand == F("bgcolor")) {
					hex2rrggbb(parseString(string, 3));
				}

				else if (subCommand == F("count")) {
					count = parseString(string, 3).toInt();
				}

				else if (subCommand == F("on") || subCommand == F("off")) {

					fadetime = 1000;
					fadedelay = 0;

					fadetime = (parseString(string, 3) == "")
					? fadetime
					: parseString(string, 3).toInt();
					fadedelay = (parseString(string, 4) == "")
					? fadedelay
					: parseString(string, 4).toInt();

					for (int pixel = 0; pixel < pixelCount; pixel++) {

						r_pixel = (fadedelay < 0)
						? pixelCount - pixel - 1
						: pixel;

						starttime[r_pixel] = counter20ms + (pixel * abs(fadedelay) / 20);

						if ( subCommand == F("on") && mode == Off ) { 						// switch on
							rgb_target[pixel] = rgb_old[pixel];
							rgb_old[pixel] = Plugin_124_pixels->GetPixelColor(pixel);
						} else if ( subCommand == F("off") ) { 		// switch off
							rgb_old[pixel] = Plugin_124_pixels->GetPixelColor(pixel);
							rgb_target[pixel] = RgbColor(0,0,0);
						}
					}

					if ( ( subCommand == F("on") ) && mode == Off ) { // switch on
						mode = ( savemode == On ) ? Fade : savemode;
					} else if ( subCommand == F("off") ) { // switch off
						savemode = mode;
						mode = Fade;
					}

					maxtime = starttime[r_pixel] + (fadetime / 20);
				}

				else if (subCommand == F("dim")) {
					Plugin_124_pixels->SetBrightness(parseString(string, 3).toInt());
				}

				else if (subCommand == F("line")) {
					mode = On;

					hex2rgb(parseString(string, 5));

					for (int i = parseString(string, 3).toInt() - 1; i < parseString(string, 4).toInt(); i++){
						Plugin_124_pixels->SetPixelColor(i, rgb);
					}
				}

				else if (subCommand == F("one")) {
					mode = On;

					uint8_t pixnum = parseString(string, 3).toInt() - 1;
					hex2rgb(parseString(string, 4));

					Plugin_124_pixels->SetPixelColor(pixnum, rgb);
				}

				else if (subCommand == F("fade") || subCommand == F("all") || subCommand == F("rgb")) {
					mode = Fade;

					if (subCommand == F("all") || subCommand == F("rgb")) {
						fadedelay = 0;
					}

					hex2rgb_pixel(parseString(string, 3));

					fadetime = (parseString(string, 4) == "")
					? fadetime
					: parseString(string, 4).toInt();
					fadedelay = (parseString(string, 5) == "")
					? fadedelay
					: parseString(string, 5).toInt();

					uint8_t r_pixel;

					for (int pixel = 0; pixel < pixelCount; pixel++){

						r_pixel = (fadedelay < 0)
						? pixelCount - pixel - 1
						: pixel;

						starttime[r_pixel] = counter20ms + (pixel * abs(fadedelay) / 20);

						rgb_old[pixel] = Plugin_124_pixels->GetPixelColor(pixel);
					}
					maxtime = starttime[r_pixel] + (fadetime / 20);
				}

				else if (subCommand == F("rainbow")) {
					mode = Rainbow;

					rainbowspeed = (parseString(string, 3) == "")
					? 1
					: parseString(string, 3).toInt();
				}

				else if (subCommand == F("colorfade")) {
					mode = ColorFade;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") hex2rrggbb(parseString(string, 4));

					startpixel = (parseString(string, 5) == "")
					? 0
					: parseString(string, 5).toInt() - 1;
					endpixel = (parseString(string, 6) == "")
					? pixelCount
					: parseString(string, 6).toInt();
				}

				else if (subCommand == F("kitt")) {
					mode = Kitt;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));

					speed = (parseString(string, 4) == "")
					? defaultspeed
					: parseString(string, 4).toInt();
				}

				else if (subCommand == F("comet")) {
					mode = Comet;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));

					speed = (parseString(string, 4) == "")
					? defaultspeed
					: parseString(string, 4).toInt();
				}

				else if (subCommand == F("theatre")) {
					mode = Theatre;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") hex2rrggbb(parseString(string, 4));

					count = (parseString(string, 5) == "")
					? count
					: parseString(string, 5).toInt();

					speed = (parseString(string, 6) == "")
					? defaultspeed
					: parseString(string, 6).toInt();

					for ( int i = 0; i < pixelCount; i++ ) {
						if ((i / count) % 2 == 0) {
							Plugin_124_pixels->SetPixelColor(i, rgb);
						} else {
							Plugin_124_pixels->SetPixelColor(i, rrggbb);
						}
					}
				}

				else if (subCommand == F("scan")) {
					mode = Scan;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") hex2rrggbb(parseString(string, 4));

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();
				}

				else if (subCommand == F("dualscan")) {
					mode = Dualscan;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") hex2rrggbb(parseString(string, 4));

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();
				}

				else if (subCommand == F("twinkle")) {
					mode = Twinkle;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") hex2rrggbb(parseString(string, 4));

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();
				}

				else if (subCommand == F("twinklefade")) {
					mode = TwinkleFade;

					hex2rgb(parseString(string, 3));

					count = (parseString(string, 4) == "")
					? count
					: parseString(string, 4).toInt();

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();

				}

				else if (subCommand == F("sparkle")) {
					mode = Sparkle;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));
					hex2rrggbb(parseString(string, 4));

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();
				}

				else if (subCommand == F("wipe")) {
					mode = Wipe;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") {
						hex2rrggbb(parseString(string, 4));
					} else {
						hex2rrggbb("000000");
					}

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();
				}

				else if (subCommand == F("dualwipe")) {
					mode = Dualwipe;

					_counter_mode_step = 0;

					hex2rgb(parseString(string, 3));
					if (parseString(string, 4) != "") {
						hex2rrggbb(parseString(string, 4));
					} else {
						hex2rrggbb("000000");
					}

					speed = (parseString(string, 5) == "")
					? defaultspeed
					: parseString(string, 5).toInt();
				}

				else if (subCommand == F("faketv")) {
					mode = FakeTV;
					_counter_mode_step = 0;

					randomSeed(analogRead(A0));
					pixelNum = random(numPixels); // Begin at random point

					startpixel = (parseString(string, 3) == "")
					? 0
					: parseString(string, 3).toInt() - 1;
					endpixel = (parseString(string, 4) == "")
					? pixelCount
					: parseString(string, 4).toInt();
				}

				else if (subCommand == F("fire")) {
					mode = Fire;

					fps = (parseString(string, 3) == "")
					? fps
					: parseString(string, 3).toInt();

					fps = (fps == 0 || fps > 50) ? 50	: fps;

					brightness = (parseString(string, 4) == "")
					? brightness
					: parseString(string, 4).toFloat();
					cooling = (parseString(string, 5) == "")
					? cooling
					: parseString(string, 5).toFloat();
					sparking = (parseString(string, 6) == "")
					? sparking
					: parseString(string, 6).toFloat();
				}

				else if (subCommand == F("stop")) {
					mode = On;
				}

				else if (subCommand == F("statusrequest")) {
				}

				else if ( subCommand != F("all") && subCommand != F("line")
				&& subCommand != F("one") && subCommand != F("fade")
				&& subCommand != F("dim") && subCommand != F("fadetime")
				&& subCommand != F("speed") && subCommand != F("fadedelay")
				&& subCommand != F("count") && subCommand != F("bgcolor")
				&& subCommand != F("on") && subCommand != F("off")
				&& subCommand != F("rgb") && subCommand != F("rainbow")
				&& subCommand != F("kitt") && subCommand != F("comet")
				&& subCommand != F("theatre") && subCommand != F("scan")
				&& subCommand != F("dualscan") && subCommand != F("twinkle")
				&& subCommand != F("sparkle") && subCommand != F("fire")
				&& subCommand != F("twinklefade") && subCommand != F("stop")
				&& subCommand != F("wipe") && subCommand != F("dualwipe")
				&& subCommand != F("colorfade")
				&& subCommand != F("statusrequest") && subCommand != F("faketv")) {
					log = F("NeoPixelBus: unknown subcommand: ");
					log += subCommand;
					addLog(LOG_LEVEL_INFO, log);

					String json;
					printToWebJSON = true;
					json += F("{\n");
					json += F("\"plugin\": \"124\",\n");
					json += F("\"log\": \"");
					json += F("NeoPixelBus: unknown command: ");
					json += subCommand;
					json += F("\"\n");
					json += F("}\n");
					SendStatus(event->Source, json); // send http response to controller (JSON format)
				}
				NeoPixelSendStatus(event->Source);
			} // command neopixel

			if ( speed == 0 ) mode = On; // speed = 0 = stop mode
			speed = ( speed > SPEED_MAX || speed < -SPEED_MAX ) ? defaultspeed : speed; // avoid invalid values
			fadetime = (fadetime <= 0) ? 20 : fadetime;

			break;
		}

		case PLUGIN_FIFTY_PER_SECOND:
		{
			counter20ms++;
			lastmode = mode;

			switch (mode) {
				case Fade:
				fade();
				break;

				case ColorFade:
				colorfade();
				break;

				case Rainbow:
				rainbow();
				break;

				case Kitt:
				kitt();
				break;

				case Comet:
				comet();
				break;

				case Theatre:
				theatre();
				break;

				case Scan:
				scan();
				break;

				case Dualscan:
				dualscan();
				break;

				case Twinkle:
				twinkle();
				break;

				case TwinkleFade:
				twinklefade();
				break;

				case Sparkle:
				sparkle();
				break;

				case Fire:
				fire();
				break;

				case Wipe:
				wipe();
				break;

				case Dualwipe:
				dualwipe();
				break;

				case FakeTV:
				faketv();
				break;

				default:
				break;
			} // switch mode

			Plugin_124_pixels->Show();

			if (mode != lastmode) {
				String log = "";
				log = F("NeoPixelBus: Mode Change: ");
				log += modeName[mode];
				addLog(LOG_LEVEL_INFO, log);
				NeoPixelSendStatus(event->Source);
			}
			success = true;
			break;
		}

		break;


	}
	return success;
}

void fade(void) {
for (int pixel = 0; pixel < pixelCount; pixel++){
	long zaehler = 20 * ( counter20ms - starttime[pixel] );
	float progress = (float) zaehler / (float) fadetime ;
	progress = (progress < 0) ? 0 : progress;
	progress = (progress > 1) ? 1 : progress;

	RgbColor updatedColor = RgbColor::LinearBlend(
		rgb_old[pixel], rgb_target[pixel],
		progress);

		if ( counter20ms > maxtime && Plugin_124_pixels->GetPixelColor(pixel).R == 0 && Plugin_124_pixels->GetPixelColor(pixel).G == 0 && Plugin_124_pixels->GetPixelColor(pixel).B == 0) {
			mode = Off;
		} else if ( counter20ms > maxtime ) {
			mode = On;
		}

		Plugin_124_pixels->SetPixelColor(pixel, updatedColor);
	}
}

void colorfade(void) {
float progress = 0;
difference = abs(endpixel - startpixel);
for(uint8_t i = 0; i < difference; i++)
{

	progress = (float) i / ( difference - 1 );
	progress = (progress >= 1) ? 1 : progress;
	progress = (progress <= 0) ? 0 : progress;

	RgbColor updatedColor = RgbColor::LinearBlend(
		rgb, rrggbb,
		progress);

		Plugin_124_pixels->SetPixelColor(i + startpixel, updatedColor);
	}
	mode = On;
}

void wipe(void) {
	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0) {
		if (speed > 0) {
			Plugin_124_pixels->SetPixelColor(_counter_mode_step, rrggbb);
			if ( _counter_mode_step > 0 ) Plugin_124_pixels->SetPixelColor( _counter_mode_step - 1, rgb);
		} else {
			Plugin_124_pixels->SetPixelColor(pixelCount - _counter_mode_step - 1, rrggbb);
			if ( _counter_mode_step > 0 ) Plugin_124_pixels->SetPixelColor( pixelCount - _counter_mode_step, rgb);
		}
		if ( _counter_mode_step == pixelCount ) mode = On;
		_counter_mode_step++;
	}
}

void dualwipe(void) {
	if (counter20ms % (SPEED_MAX / abs(speed)) == 0) {
		if (speed > 0) {
			int i = _counter_mode_step - pixelCount;
			i = abs(i);
			Plugin_124_pixels->SetPixelColor(_counter_mode_step, rrggbb);
			Plugin_124_pixels->SetPixelColor(i, rgb);
			if (_counter_mode_step > 0 ) {
				Plugin_124_pixels->SetPixelColor(_counter_mode_step - 1, rgb);
				Plugin_124_pixels->SetPixelColor(i - 1, rrggbb);
			}
		} else {
			int i = (pixelCount / 2) - _counter_mode_step;
			i = abs(i);
			Plugin_124_pixels->SetPixelColor(_counter_mode_step + (pixelCount / 2), rrggbb);
			Plugin_124_pixels->SetPixelColor(i, rgb);
			if (_counter_mode_step > 0 ) {
				Plugin_124_pixels->SetPixelColor(_counter_mode_step + (pixelCount / 2) - 1, rgb);
				Plugin_124_pixels->SetPixelColor(i - 1, rrggbb);
			}
		}
		if (_counter_mode_step >= pixelCount / 2) {
			mode = On;
			Plugin_124_pixels->SetPixelColor(_counter_mode_step - 1, rgb);
		}
		_counter_mode_step++;
	}
}

void faketv(void) {
	if (counter20ms >= ftv_holdTime) {

		uint32_t ftv_totalTime, ftv_fadeTime, ftv_startTime, ftv_elapsed;
		uint16_t ftv_nr, ftv_ng, ftv_nb, ftv_r, ftv_g, ftv_b, ftv_i;
		uint8_t  ftv_hi, ftv_lo, ftv_r8, ftv_g8, ftv_b8, ftv_frac;
		difference = abs(endpixel - startpixel);

		if (ftv_elapsed >= ftv_fadeTime) {
			// Read next 16-bit (5/6/5) color
			ftv_hi = pgm_read_byte(&ftv_colors[pixelNum * 2    ]);
			ftv_lo = pgm_read_byte(&ftv_colors[pixelNum * 2 + 1]);
			if(++pixelNum >= numPixels) pixelNum = 0;

			// Expand to 24-bit (8/8/8)
			ftv_r8 = (ftv_hi & 0xF8) | (ftv_hi >> 5);
			ftv_g8 = (ftv_hi << 5) | ((ftv_lo & 0xE0) >> 3) | ((ftv_hi & 0x06) >> 1);
			ftv_b8 = (ftv_lo << 3) | ((ftv_lo & 0x1F) >> 2);
			// Apply gamma correction, further expand to 16/16/16
			ftv_nr = (uint8_t)pgm_read_byte(&ftv_gamma8[ftv_r8]) * 257; // New R/G/B
			ftv_ng = (uint8_t)pgm_read_byte(&ftv_gamma8[ftv_g8]) * 257;
			ftv_nb = (uint8_t)pgm_read_byte(&ftv_gamma8[ftv_b8]) * 257;

			ftv_totalTime = random(12, 125);    // Semi-random pixel-to-pixel time
			ftv_fadeTime  = random(0, ftv_totalTime); // Pixel-to-pixel transition time
			if(random(10) < 3) ftv_fadeTime = 0;  // Force scene cut 30% of time
			ftv_holdTime  = counter20ms + ftv_totalTime - ftv_fadeTime; // Non-transition time
			ftv_startTime = counter20ms;
		}

		ftv_elapsed = counter20ms - ftv_startTime;
		if(ftv_fadeTime) {
			ftv_r = map(ftv_elapsed, 0, ftv_fadeTime, ftv_pr, ftv_nr); // 16-bit interp
			ftv_g = map(ftv_elapsed, 0, ftv_fadeTime, ftv_pg, ftv_ng);
			ftv_b = map(ftv_elapsed, 0, ftv_fadeTime, ftv_pb, ftv_nb);
		} else { // Avoid divide-by-zero in map()
			ftv_r = ftv_nr;
			ftv_g = ftv_ng;
			ftv_b = ftv_nb;
		}

		for(ftv_i=0; ftv_i<difference; ftv_i++) {
			ftv_r8   = ftv_r >> 8; // Quantize to 8-bit
			ftv_g8   = ftv_g >> 8;
			ftv_b8   = ftv_b >> 8;
			ftv_frac = (ftv_i << 8) / difference; // LED index scaled to 0-255
			if((ftv_r8 < 255) && ((ftv_r & 0xFF) >= ftv_frac)) ftv_r8++; // Boost some fraction
			if((ftv_g8 < 255) && ((ftv_g & 0xFF) >= ftv_frac)) ftv_g8++; // of LEDs to handle
			if((ftv_b8 < 255) && ((ftv_b & 0xFF) >= ftv_frac)) ftv_b8++; // interp > 8bit
			Plugin_124_pixels->SetPixelColor(ftv_i + startpixel, RgbColor(ftv_r8, ftv_g8, ftv_b8));
		}

		ftv_pr = ftv_nr; // Prev RGB = new RGB
		ftv_pg = ftv_ng;
		ftv_pb = ftv_nb;
	}
}

/*
* Cycles a rainbow over the entire string of LEDs.
*/
void rainbow(void) {
	for(int i=0; i< pixelCount; i++)
	{
		uint8_t r1 = (Wheel(((i * 256 / pixelCount) + counter20ms * rainbowspeed) & 255) >> 16);
		uint8_t g1 = (Wheel(((i * 256 / pixelCount) + counter20ms * rainbowspeed) & 255) >> 8);
		uint8_t b1 = (Wheel(((i * 256 / pixelCount) + counter20ms * rainbowspeed) & 255));
		Plugin_124_pixels->SetPixelColor(i, RgbColor(r1, g1, b1));
	}
	mode = (rainbowspeed == 0) ? On : Rainbow;
}


/*
* Put a value 0 to 255 in to get a color value.
* The colours are a transition r -> g -> b -> back to r
* Inspired by the Adafruit examples.
*/
uint32_t Wheel(uint8_t pos) {
	pos = 255 - pos;
	if(pos < 85) {
		return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (pos * 3);
	} else if(pos < 170) {
		pos -= 85;
		return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
	} else {
		pos -= 170;
		return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (0);
	}
}


// Larson Scanner K.I.T.T.
void kitt(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0)
	{
		for(uint16_t i=0; i < pixelCount; i++) {
			RgbColor px_rgb = Plugin_124_pixels->GetPixelColor(i);

			// fade out (divide by 2)
			px_rgb.R = px_rgb.R >> 1;
			px_rgb.G = px_rgb.G >> 1;
			px_rgb.B = px_rgb.B >> 1;

			Plugin_124_pixels->SetPixelColor(i, px_rgb);
		}

		uint16_t pos = 0;

		if(_counter_mode_step < pixelCount) {
			pos = _counter_mode_step;
		} else {
			pos = (pixelCount * 2) - _counter_mode_step - 2;
		}

		Plugin_124_pixels->SetPixelColor(pos, rgb);

		_counter_mode_step = (_counter_mode_step + 1) % ((pixelCount * 2) - 2);
	}
}


//Firing comets from one end.
void comet(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0)
	{
		for(uint16_t i=0; i < pixelCount; i++) {

			if (speed > 0) {

				byte px_r = Plugin_124_pixels->GetPixelColor(i).R;
				byte px_g = Plugin_124_pixels->GetPixelColor(i).G;
				byte px_b = Plugin_124_pixels->GetPixelColor(i).B;

				// fade out (divide by 2)
				px_r = px_r >> 1;
				px_g = px_g >> 1;
				px_b = px_b >> 1;

				Plugin_124_pixels->SetPixelColor(i, RgbColor(px_r, px_g, px_b));

			} else {

				byte px_r = Plugin_124_pixels->GetPixelColor(pixelCount -i -1).R;
				byte px_g = Plugin_124_pixels->GetPixelColor(pixelCount -i -1).G;
				byte px_b = Plugin_124_pixels->GetPixelColor(pixelCount -i -1).B;

				// fade out (divide by 2)
				px_r = px_r >> 1;
				px_g = px_g >> 1;
				px_b = px_b >> 1;

				Plugin_124_pixels->SetPixelColor(pixelCount -i -1, RgbColor(px_r, px_g, px_b));
			}
		}

		if (speed > 0) {
			Plugin_124_pixels->SetPixelColor(_counter_mode_step, rgb);
		} else {
			Plugin_124_pixels->SetPixelColor(pixelCount - _counter_mode_step -1, rgb);
		}

		_counter_mode_step = (_counter_mode_step + 1) % pixelCount;
	}
}


//Theatre lights
void theatre(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0 && speed != 0)
	{
		if (speed > 0) {
			Plugin_124_pixels->RotateLeft(1,0,(pixelCount/count)*count-1);
		} else {
			Plugin_124_pixels->RotateRight(1,0,(pixelCount/count)*count-1);
		}
	}
}


/*
* Runs a single pixel back and forth.
*/
void scan(void) {
	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0 && speed != 0)
	{
		if(_counter_mode_step > (pixelCount*2) - 2) {
			_counter_mode_step = 0;
		}
		_counter_mode_step++;

		int i = _counter_mode_step - (pixelCount - 1);
		i = abs(i);

		//Plugin_124_pixels->ClearTo(rrggbb);
		for (int i = 0; i < pixelCount; i++) Plugin_124_pixels->SetPixelColor(i,rrggbb);
		Plugin_124_pixels->SetPixelColor(abs(i), rgb);
	}

}


/*
* Runs two pixel back and forth in opposite directions.
*/
void dualscan(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0 && speed != 0) {
		if(_counter_mode_step > (pixelCount*2) - 2) {
			_counter_mode_step = 0;
		}

		_counter_mode_step++;

		int i = _counter_mode_step - (pixelCount - 1);
		i = abs(i);

		//Plugin_124_pixels->ClearTo(rrggbb);
		for (int i = 0; i < pixelCount; i++) Plugin_124_pixels->SetPixelColor(i,rrggbb);
		Plugin_124_pixels->SetPixelColor(abs(i), rgb);
		Plugin_124_pixels->SetPixelColor(pixelCount - (i+1), rgb);

	}
}


/*
* Blink several LEDs on, reset, repeat.
* Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
*/
void twinkle(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0 && speed != 0)
	{
		if(_counter_mode_step == 0) {
			//Plugin_124_pixels->ClearTo(rrggbb);
			for (int i = 0; i < pixelCount; i++) Plugin_124_pixels->SetPixelColor(i,rrggbb);
			uint16_t min_leds = _max(1, pixelCount/5); // make sure, at least one LED is on
			uint16_t max_leds = _max(1, pixelCount/2); // make sure, at least one LED is on
			_counter_mode_step = random(min_leds, max_leds);
		}

		Plugin_124_pixels->SetPixelColor(random(pixelCount), rgb);

		_counter_mode_step--;
	}
}


/*
* Blink several LEDs on, fading out.
*/
void twinklefade(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0 && speed != 0)
	{
		for(uint16_t i=0; i < pixelCount; i++) {

			byte px_r = Plugin_124_pixels->GetPixelColor(pixelCount -i -1).R;
			byte px_g = Plugin_124_pixels->GetPixelColor(pixelCount -i -1).G;
			byte px_b = Plugin_124_pixels->GetPixelColor(pixelCount -i -1).B;

			// fade out (divide by 2)
			px_r = px_r >> 1;
			px_g = px_g >> 1;
			px_b = px_b >> 1;

			Plugin_124_pixels->SetPixelColor(i, RgbColor(px_r, px_g, px_b));
		}

		if(random(count) < 50) {
			Plugin_124_pixels->SetPixelColor(random(pixelCount), rgb);
		}
	}
}


/*
* Blinks one LED at a time.
* Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
*/
void sparkle(void) {

	if (counter20ms % ( SPEED_MAX / abs(speed) ) == 0 && speed != 0)
	{
		//Plugin_124_pixels->ClearTo(rrggbb);
		for (int i = 0; i < pixelCount; i++) Plugin_124_pixels->SetPixelColor(i,rrggbb);
		Plugin_124_pixels->SetPixelColor(random(pixelCount), rgb);
	}
}


//Fire
long fireTimer;
CRGB leds[ARRAYSIZE];

void fire(void) {

	if (counter20ms > fireTimer + 50 / fps) {
		fireTimer = counter20ms;
		Fire2012();
		RgbColor pixel;

		for (int i = 0; i < pixelCount; i++) {
			pixel = RgbColor(leds[i].r, leds[i].g, leds[i].b);
			pixel = RgbColor::LinearBlend(pixel, RgbColor(0, 0, 0), (255 - brightness)/255.0);
			Plugin_124_pixels->SetPixelColor(i, pixel);
		}
	}
}

void Fire2012(void) {
	// Array of temperature readings at each simulation cell
	static byte heat[ARRAYSIZE];

	// Step 1.  Cool down every cell a little
	for ( int i = 0; i < pixelCount; i++) {
		heat[i] = qsub8( heat[i],  random8(0, ((cooling * 10) / pixelCount) + 2));
	}

	// Step 2.  Heat from each cell drifts 'up' and diffuses a little
	for ( int k = pixelCount - 1; k >= 2; k--) {
		heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
	}

	// Step 3.  Randomly ignite new 'sparks' of heat near the bottom
	if ( random8() < sparking ) {
		int y = random8(7);
		heat[y] = qadd8( heat[y], random8(160, 255) );
	}

	// Step 4.  Map from heat cells to LED colors
	for ( int j = 0; j < pixelCount; j++) {
		CRGB color = HeatColor( heat[j]);
		int pixelnumber;
		if ( gReverseDirection ) {
			pixelnumber = (pixelCount - 1) - j;
		} else {
			pixelnumber = j;
		}
		leds[pixelnumber] = color;
	}
}


int32_t rgbStr2Num(String rgbStr) {
	int32_t rgbDec = (int) strtol( &rgbStr[0], NULL, 16);
	return rgbDec;
}


void hex2rgb(String hexcolor) {
	colorStr = hexcolor;
	rgb = RgbColor ( rgbStr2Num(hexcolor) >> 16, rgbStr2Num(hexcolor) >> 8, rgbStr2Num(hexcolor) );
}

void hex2rrggbb(String hexcolor) {
	backgroundcolorStr = hexcolor;
	rrggbb = RgbColor ( rgbStr2Num(hexcolor) >> 16, rgbStr2Num(hexcolor) >> 8, rgbStr2Num(hexcolor) );
}

void hex2rgb_pixel(String hexcolor) {
	colorStr = hexcolor;
	for ( int i = 0; i < pixelCount; i++) {
		rgb_target[i] = RgbColor ( rgbStr2Num(hexcolor) >> 16, rgbStr2Num(hexcolor) >> 8, rgbStr2Num(hexcolor) );
	}
}

// ---------------------------------------------------------------------------------
// ------------------------------ JsonResponse -------------------------------------
// ---------------------------------------------------------------------------------
void NeoPixelSendStatus(byte eventSource) {
	String log = String(F("NeoPixelBusFX: Set ")) + rgb.R
	+ String(F("/")) + rgb.G + String(F("/")) + rgb.B;
	addLog(LOG_LEVEL_INFO, log);

	String json;
	printToWebJSON = true;
	json += F("{\n");
	json += F("\"plugin\": \"124");
	json += F("\",\n\"mode\": \"");
	json += modeName[mode];
	json += F("\",\n\"lastmode\": \"");
	json += modeName[savemode];
	json += F("\",\n\"fadetime\": \"");
	json += fadetime;
	json += F("\",\n\"fadedelay\": \"");
	json += fadedelay;
	json += F("\",\n\"dim\": \"");
	json += Plugin_124_pixels->GetBrightness();
	json += F("\",\n\"rgb\": \"");
	json += colorStr;
	json += F("\",\n\"bgcolor\": \"");
	json += backgroundcolorStr;
	json += F("\",\n\"pixelcount\": \"");
	json += pixelCount;
	json += F("\"\n}\n");
	SendStatus(eventSource, json); // send http response to controller (JSON format)
}
