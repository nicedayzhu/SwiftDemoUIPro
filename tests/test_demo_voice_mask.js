"use strict";

var fs = require("fs");
var path = require("path");
var vm = require("vm");

var sourcePath = path.join(__dirname, "..", "addon", "panorama", "scripts", "hud", "swift_demo_voice.js");
var source = fs.readFileSync(sourcePath, "utf8");
var layoutPath = path.join(__dirname, "..", "addon", "panorama", "layout", "hud", "huddemocontroller.xml");
var layout = fs.readFileSync(layoutPath, "utf8");
var stylePath = path.join(__dirname, "..", "addon", "panorama", "styles", "hud", "swift_demo_voice.css");
var style = fs.readFileSync(stylePath, "utf8");
var nativeLayoutPath = path.join(__dirname, "..", "..", "res_panorama", "panorama", "layout", "hud", "huddemocontroller.xml");
var nativeLayout = fs.readFileSync(nativeLayoutPath, "utf8");

function extractNativeRoot(xml) {
	var start = xml.indexOf('<Panel id="Root">');
	if (start < 0) throw new Error("native DemoUI Root panel not found");
	var tokenPattern = /<\/?Panel\b[^>]*>/g;
	tokenPattern.lastIndex = start;
	var depth = 0;
	var token = null;
	while ((token = tokenPattern.exec(xml))) {
		if (/^<\/Panel/.test(token[0])) depth--;
		else if (!/\/>$/.test(token[0])) depth++;
		if (depth === 0) return xml.slice(start, tokenPattern.lastIndex).replace(/\r\n/g, "\n");
	}
	throw new Error("native DemoUI Root panel is incomplete");
}

if (/[^\x00-\x7f]/.test(source)) {
	throw new Error("Panorama voice runtime must remain ASCII-only to avoid ResourceCompiler encoding corruption");
}

if (/DemoController(?:Hidden|Full|Minimal)/.test(source)) {
	throw new Error("custom runtime must not mutate native DemoController mode classes");
}

if (!/\$\.Schedule\(0\.0, SwiftDemoVoice\.OnLoad\)/.test(source)) {
	throw new Error("custom runtime must self-start without relying only on XML onload");
}

if (!/var SwiftDemoVoice\s*=\s*\(function\s*\(\)/.test(source) || /var SwiftDemoVoice\s*;/.test(source)) {
	throw new Error("Panorama runtime must export SwiftDemoVoice with one atomic global assignment");
}

if (!/id="SwiftDemoVoiceMenu"\s+class="[^"]*\bdemo-active\b/.test(layout)) {
	throw new Error("menu layout must fail open as visible while the runtime starts");
}

if (!/<Panel id="Root">/.test(layout) || /swift-demo-native-bar-suppressed/.test(layout)) {
	throw new Error("Valve's native DemoUI Root must remain intact");
}

if (/swift-demo-playback/.test(layout)) {
	throw new Error("custom menu must not duplicate native playback controls");
}

if (extractNativeRoot(layout) !== extractNativeRoot(nativeLayout)) {
	throw new Error("Valve's native DemoUI Root must remain byte-for-byte unchanged");
}

if (/swift-demo-playback|#(?:Contents|PlayButton|SliderRow|Slider|RoundMarkers|HighlightMarkers|HighlightIcons|ControlRow|TimeControls|RoundControls|RoundPrev|RoundRestart|RoundNumber|HotKeyLabels|Settings)\b/.test(style)) {
	throw new Error("custom stylesheet must not target native DemoUI controls");
}

if (/SwiftDemoVoiceActivity|SwiftDemoVoiceSpeaking|NOW SPEAKING|hudvoicestatus/.test(layout) || /swift-demo-voice-activity|player\.speaking|player__speaking/.test(style) || /_PollVoiceActivity|VoiceNotice|VoiceTextMatchesPlayer/.test(source)) {
	throw new Error("live speaker UI and polling must remain removed");
}

if (!/spec_mode 2/.test(source) || /spec_mode 4/.test(source)) {
	throw new Error("player focus must use CS2 OBS_MODE_IN_EYE (spec_mode 2)");
}

if (/RegisterKeyBind|bind\s+[\"']?space/i.test(source)) {
	throw new Error("custom menu must not override the native Space camera control");
}

if (!/bluedots_large_png\.vtex/.test(style) || !/border-left:\s*3px solid #e4ae39/.test(style)) {
	throw new Error("custom menu must retain the native-inspired Swift Menu chrome");
}

if (!/icons\/ui\/unmuted\.vsvg/.test(layout) || !/SwiftDemoVoiceEnabledLabel/.test(layout) || /icons\/ui\/sound_3\.vsvg/.test(layout)) {
	throw new Error("voice controls must use explicit unmuted/muted UI with a text state");
}

var commands = [];
var startupClasses = {};
var menuPanel = {
	SetHasClass: function (name, enabled) { startupClasses[name] = enabled; }
};
var contextPanel = {
	FindChildTraverse: function (id) { return id === "SwiftDemoVoiceMenu" ? menuPanel : null; },
	IsPlayingDemo: function () { return true; }
};
var context = {
	console: console,
	isFinite: isFinite,
	$: {
		GetContextPanel: function () { return contextPanel; },
		Schedule: function (delay, callback) { if (delay === 0 && callback) callback(); },
		Msg: function () {}
	},
	GameStateAPI: {},
	GameInterfaceAPI: { ConsoleCommand: function (command) { commands.push(command); } }
};
vm.createContext(context);
vm.runInContext(source, context, { filename: sourcePath });

if (startupClasses["demo-active"] !== true || startupClasses.collapsed !== false) {
	throw new Error("self-start must make the menu visible and expanded");
}

function assertMasks(slots, expectedLow, expectedHigh) {
	var actual = context.SwiftDemoVoice.BuildMasksForSlots(slots);
	if (actual.low !== expectedLow || actual.high !== expectedHigh) {
		throw new Error(
			"slots " + JSON.stringify(slots) +
			" expected " + expectedLow + "/" + expectedHigh +
			" but got " + actual.low + "/" + actual.high
		);
	}
}

assertMasks([], 0, 0);
assertMasks([0], 1, 0);
assertMasks([31], -2147483648, 0);
assertMasks([32], 0, 1);
assertMasks([63], 0, -2147483648);
assertMasks([3, 4, 8, 10, 11], 3352, 0);
assertMasks(Array.from({ length: 64 }, function (_, index) { return index; }), -1, -1);
assertMasks([-1, 64, "bad", 0, 0], 1, 0);

context.GameStateAPI.GetPlayerDataJSO = function () {
	return {
		teams: [{ name: "" }, { name: "TERRORIST" }, { name: "CT" }],
		players: [
			{ xuid: "101", team: 1 },
			{ xuid: "202", team: 2, slot: 8, name: "Fallback Name" }
		]
	};
};
context.GameStateAPI.GetPlayerSlot = function (xuid) { return xuid === "101" ? 3 : -1; };
context.GameStateAPI.GetPlayerName = function (xuid) { return xuid === "101" ? "Primary Name" : ""; };
context.GameStateAPI.GetPlayerTeamName = function () { return ""; };
context.GameStateAPI.GetPlayerStatsJSO = function (xuid) { return { status: xuid === "202" ? 1 : 0 }; };
var players = context.SwiftDemoVoice.ReadPlayersForTest();
if (players.length !== 2 || players[0].slot !== 3 || players[0].team !== "TERRORIST" || players[1].slot !== 8 || players[1].name !== "Fallback Name" || !players[0].canFocus || !players[1].isDead || players[1].canFocus) {
	throw new Error("demo player discovery/fallback failed: " + JSON.stringify(players));
}
context.SwiftDemoVoice.Refresh(false);

commands.length = 0;
if (context.SwiftDemoVoice.AccountIdFromXuid("76561198000000123") !== 39734395) {
	throw new Error("Steam account ID conversion failed");
}

context.SwiftDemoVoice.FocusPlayer(8, "Fallback Name", "202");
if (commands.length !== 0) {
	throw new Error("dead players must not issue POV commands: " + JSON.stringify(commands));
}

context.SwiftDemoVoice.FocusPlayer(12, "Example Player", "76561198000000123");
if (commands[0] !== "spec_mode 2" || commands[1] !== "spec_lock_to_accountid 39734395" || commands[2] !== "spec_player 13" || commands.indexOf("spec_mode 4") !== -1) {
	throw new Error("POV command mapping failed: " + JSON.stringify(commands));
}

console.log("demo voice mask, native UI, and first-person POV tests passed");
