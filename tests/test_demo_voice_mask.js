"use strict";

var fs = require("fs");
var path = require("path");
var vm = require("vm");
var crypto = require("crypto");

var sourcePath = path.join(__dirname, "..", "addon", "panorama", "scripts", "hud", "swift_demo_voice.js");
var source = fs.readFileSync(sourcePath, "utf8");
var dataSourcePath = path.join(__dirname, "..", "addon", "panorama", "scripts", "hud", "swift_demo_voice_data.js");
var dataSource = fs.readFileSync(dataSourcePath, "utf8");
var layoutPath = path.join(__dirname, "..", "addon", "panorama", "layout", "hud", "huddemocontroller.xml");
var layout = fs.readFileSync(layoutPath, "utf8");
var stylePath = path.join(__dirname, "..", "addon", "panorama", "styles", "hud", "swift_demo_voice.css");
var style = fs.readFileSync(stylePath, "utf8");
var localizationDir = path.join(__dirname, "..", "addon", "resource");
var englishLocalization = fs.readFileSync(path.join(localizationDir, "platform_english.txt"), "utf8");
var schineseLocalization = fs.readFileSync(path.join(localizationDir, "platform_schinese.txt"), "utf8");
var buildScript = fs.readFileSync(path.join(__dirname, "..", "powershell", "build_demo_menu_override.ps1"), "utf8");

function localizationTokens(text) {
	var result = {};
	var pattern = /^\s*"(SwiftDemoVoice_[A-Za-z0-9_]+)"\s+"((?:[^"\\]|\\.)*)"\s*$/gm;
	var match = null;
	while ((match = pattern.exec(text))) result[match[1]] = match[2];
	return result;
}

var englishTokens = localizationTokens(englishLocalization);
var schineseTokens = localizationTokens(schineseLocalization);
var englishKeys = Object.keys(englishTokens).sort();
var schineseKeys = Object.keys(schineseTokens).sort();
if (englishKeys.length === 0 || JSON.stringify(englishKeys) !== JSON.stringify(schineseKeys)) {
	throw new Error("English and Simplified Chinese localization catalogs must contain the same token set");
}
englishKeys.forEach(function (token) {
	var placeholderPattern = /\{[sd]:[A-Za-z0-9_-]+\}/g;
	var englishPlaceholders = (englishTokens[token].match(placeholderPattern) || []).sort();
	var schinesePlaceholders = (schineseTokens[token].match(placeholderPattern) || []).sort();
	if (JSON.stringify(englishPlaceholders) !== JSON.stringify(schinesePlaceholders)) {
		throw new Error("localization placeholders must match for token: " + token);
	}
});

if (!/^\s*"Tokens"\s*\{/m.test(englishLocalization)
	|| !/^\s*"Tokens"\s*\{/m.test(schineseLocalization)
	|| /"lang"\s*\{/.test(englishLocalization + schineseLocalization)) {
	throw new Error("CS2 platform localization catalogs must use a top-level Tokens block");
}

var referencedTokens = {};
var referencePattern = /#(SwiftDemoVoice_[A-Za-z0-9_]+)/g;
var referenceMatch = null;
while ((referenceMatch = referencePattern.exec(layout + "\n" + source))) referencedTokens[referenceMatch[1]] = true;
englishKeys.forEach(function (token) {
	if (!referencedTokens[token]) throw new Error("unused Demo Voice localization token: " + token);
});
Object.keys(referencedTokens).forEach(function (token) {
	if (!englishTokens[token] || !schineseTokens[token]) throw new Error("missing Demo Voice localization token: " + token);
});

if (!/Join-Path \$gameAddon "resource"/.test(buildScript)
	|| !/platform_english\.txt/.test(buildScript)
	|| !/platform_schinese\.txt/.test(buildScript)
	|| !/Get-Content -Raw -Encoding UTF8/.test(buildScript)
	|| !/UTF8Encoding\(\$true\)/.test(buildScript)) {
	throw new Error("build must copy UTF-8 BOM CS2 platform localization catalogs into the game resource directory");
}

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

if (/[^\x00-\x7f]/.test(source + dataSource)) {
	throw new Error("Panorama voice runtime must remain ASCII-only to avoid ResourceCompiler encoding corruption");
}

if (/(?:SetHasClass|AddClass|RemoveClass|SwitchClass)\([^)]*DemoController(?:Hidden|Full|Minimal)/.test(source)) {
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

var nativeRootHash = crypto.createHash("sha256").update(extractNativeRoot(layout)).digest("hex");
if (nativeRootHash !== "cc5a10b29e1abdbd65b2a9260a6b0d55784aac87534629890bdc76072d4efd94") {
    throw new Error("Valve's native DemoUI Root must remain byte-for-byte unchanged: " + nativeRootHash);
}

var menuWidth = /\.swift-demo-voice\s*\{[^}]*\bwidth:\s*(\d+)px;/m.exec(style);
var menuHeight = /\.swift-demo-voice\s*\{[^}]*\bheight:\s*(\d+)px;/m.exec(style);
if (!menuWidth || Number(menuWidth[1]) > 360 || !menuHeight || Number(menuHeight[1]) > 650) {
	throw new Error("Demo Voice panel must keep a compact right-side footprint");
}

var menuMargins = /\.swift-demo-voice\s*\{[^}]*\bhorizontal-align:\s*right;[^}]*\bmargin:\s*0px\s+(\d+)px\s+(\d+)px\s+0px;/m.exec(style);
if (!menuMargins || Number(menuMargins[1]) < 280 || Number(menuMargins[2]) < 212) {
	throw new Error("Demo Voice panel must sit above the native bottom-right equipment strip");
}

if (!/\.swift-demo-voice\s*\{[^}]*\btransform-origin:\s*100%\s+50%;/m.test(style)
	|| !/\.swift-demo-voice\.swift-aspect-4x3\s*\{[^}]*\bmargin-right:\s*210px;[^}]*\bscaleX\(0\.75\);/m.test(style)
	|| !/\.swift-demo-voice\.swift-aspect-4x3\.demo-active\s*\{[^}]*\bscaleX\(0\.75\);/m.test(style)
	|| !/_UpdateViewportClass\(\)/.test(source)
	|| !/actuallayoutwidth/.test(source)
	|| !/actualuiscale_x/.test(source)) {
	throw new Error("4:3 Demo Voice layout must compensate for native horizontal stretching");
}

if (!/\.DemoControllerMinimal \.swift-demo-voice\s*\{[^}]*\bmargin-bottom:\s*166px;/m.test(style)
	|| !/\.DemoControllerHidden \.swift-demo-voice\s*\{[^}]*\bmargin-bottom:\s*118px;/m.test(style)) {
	throw new Error("every native Demo controller mode must preserve equipment-strip clearance");
}

if (!/id="SwiftDemoVoiceHeader"[^>]*hittestchildren="false"[^>]*onactivate="SwiftDemoVoice\.ToggleOpen\(\)"/.test(layout)
	|| /class="swift-demo-voice__toggle"/.test(layout)) {
	throw new Error("the full Demo Voice title bar must be the single expand/collapse target");
}

if (/draggable="true"|SwiftDemoVoiceDrag|SwiftDemoVoiceResetPosition/.test(layout)
	|| /RegisterEventHandler\("Drag(?:Start|End)"|SetPositionInPixels|ResetPosition/.test(source)) {
	throw new Error("Demo Voice panel must remain fixed and must not include drag or reset-position behavior");
}

if (/swift-demo-playback|#(?:Contents|PlayButton|SliderRow|Slider|RoundMarkers|HighlightMarkers|HighlightIcons|ControlRow|TimeControls|RoundControls|RoundPrev|RoundRestart|RoundNumber|HotKeyLabels|Settings)\b/.test(style)) {
	throw new Error("custom stylesheet must not target native DemoUI controls");
}

if (!/swift_demo_voice_data\.vjs_c/.test(layout)
	|| !/id="SwiftDemoVoiceStatusOverlay"/.test(layout)
	|| !/id="SwiftDemoVoiceNoticeList"/.test(layout)
	|| !/id="SwiftDemoVoiceIndexStatus"/.test(layout)
	|| !/SwiftDemoSpeakingPlayer/.test(layout)
	|| !/_PollVoiceActivity/.test(source)
	|| !/_UpdateVoiceIndexStatus/.test(source)
	|| !/_SpeakingPlayerForSlot/.test(source)
	|| !/SpeakingSlotsForTick/.test(source)
	|| !/FilterSpeakingSlotsForSelection/.test(source)
	|| !/swift-demo-speaking-player/.test(style)
	|| !/swift-demo-voice__index-status/.test(style)
	|| !/generated:\s*false/.test(dataSource)) {
	throw new Error("parsed Demo voice activity must drive the custom speaker HUD");
}

if (!/sourceDataScript/.test(buildScript)
	|| !/swift_demo_voice_data\.vjs_c/.test(buildScript)
	|| !/dataScriptInput/.test(buildScript)) {
	throw new Error("menu build must compile the fallback parsed-voice data resource");
}

if (!/spec_mode 2/.test(source) || /spec_mode 4/.test(source)) {
	throw new Error("player focus must use CS2 OBS_MODE_IN_EYE (spec_mode 2)");
}

if (/RegisterKeyBind|bind\s+[\"']?space/i.test(source)) {
	throw new Error("custom menu must not override the native Space camera control");
}

if (/bluedots_large_png\.vtex/.test(style)
	|| !/\.swift-demo-voice\s*\{[^}]*\bbackground-color:\s*#0f1012f2;/m.test(style)
	|| !/world-blur:\s*hudWorldBlur;/.test(style)
	|| !/box-shadow:\s*black 0px 0px 2px 0px;/.test(style)
	|| /\.swift-demo-voice\s*\{[^}]*\bborder-left:/m.test(style)) {
	throw new Error("custom menu must use native Demo HUD chrome with an opaque reading surface");
}

var headerHoverStyle = /\.swift-demo-voice__header:hover\s*\{([^}]*)\}/m.exec(style);
if (!headerHoverStyle
	|| !/\bbrightness:\s*1;/.test(headerHoverStyle[1])
	|| !/\bbackground-color:\s*#1b1c1ef7;/.test(headerHoverStyle[1])
	|| /#ffffff/.test(headerHoverStyle[1])) {
	throw new Error("collapsed menu hover must stay dark and must not flash white");
}

if (!/\.collapsed \.swift-demo-voice__header\s*\{[^}]*\bborder-bottom-color:\s*#00000000;/m.test(style)) {
	throw new Error("collapsed menu must hide the header divider to avoid a doubled bottom edge");
}

if (/SwiftVoiceBlue|#82b9d6|#526d7c|#536b7a|#668aa3|#6c8fa2/.test(style)) {
	throw new Error("legacy blue menu tint must not return outside subdued CT identity accents");
}

if (!/\.swift-demo-voice-player\.team-t\.observed\s*\{[^}]*#e4ae39/m.test(style)
	|| !/\.swift-demo-voice-player\.team-ct\.observed\s*\{[^}]*#78b9dc/m.test(style)
	|| !/\.team-t\.observed \.swift-demo-voice-player__name[^}]*\{[^}]*color:\s*color-T;/m.test(style)
	|| !/\.team-ct\.observed \.swift-demo-voice-player__name[^}]*\{[^}]*color:\s*color-CT;/m.test(style)
	|| !/\.team-t\.selected \.swift-demo-voice-player__toggle Image\s*\{[^}]*wash-color:\s*SwiftVoiceGold;/m.test(style)
	|| !/\.team-ct\.selected \.swift-demo-voice-player__toggle Image\s*\{[^}]*wash-color:\s*SwiftVoiceAudio;/m.test(style)) {
	throw new Error("selected and observed player accents must follow T and CT team identity colors");
}

if (!/icons\/ui\/unmuted\.vsvg/.test(layout) || !/SwiftDemoVoiceEnabledLabel/.test(layout) || /icons\/ui\/sound_3\.vsvg/.test(layout)) {
	throw new Error("voice controls must use explicit unmuted/muted UI with a text state");
}

if (!/id="SwiftDemoRoundPicker"/.test(layout) || !/SwiftDemoVoice\.ToggleRoundPicker\(\)/.test(layout) || !/#SwiftDemoVoice_Subtitle/.test(layout)) {
	throw new Error("custom menu must expose the compact round navigation component");
}

if (!/RoundIntervals/.test(source) || !/controller\.GotoTick\(Math\.floor\(rounds\[index\]\.nTickStart\)\)/.test(source)) {
	throw new Error("round navigation must use the native demo state and controller tick API");
}

if (!/#SwiftDemoVoice_FooterHint/.test(layout) || !/SwiftVoiceAudio/.test(style) || !/\$\.Localize\(token, panel\)/.test(source)) {
	throw new Error("visible Demo Voice text must use Panorama localization tokens");
}

var commands = [];
var gotoTicks = [];
var scheduledCallbacks = [];
var demoState = null;
var startupClasses = {};
var menuPanel = {
	SetHasClass: function (name, enabled) { startupClasses[name] = enabled; }
};
var contextPanel = {
	FindChildTraverse: function (id) { return id === "SwiftDemoVoiceMenu" ? menuPanel : null; },
	IsPlayingDemo: function () { return true; },
	GetDemoControllerState: function () { return demoState; },
	GotoTick: function (tick) { gotoTicks.push(tick); }
};
var context = {
	console: console,
	isFinite: isFinite,
	$: {
		GetContextPanel: function () { return contextPanel; },
		Schedule: function (delay, callback) {
			if (delay === 0 && callback) callback();
			else if (callback) scheduledCallbacks.push({ delay: delay, callback: callback });
		},
		Msg: function () {}
	},
	GameStateAPI: {},
	GameInterfaceAPI: { ConsoleCommand: function (command) { commands.push(command); } }
};
vm.createContext(context);
vm.runInContext(dataSource, context, { filename: dataSourcePath });
vm.runInContext(source, context, { filename: sourcePath });

if (context.SwiftDemoVoice.ViewportProfileForTest(1440, 1080) !== "4x3"
	|| context.SwiftDemoVoice.ViewportProfileForTest(1280, 1024) !== "4x3"
	|| context.SwiftDemoVoice.ViewportProfileForTest(1920, 1080) !== "wide"
	|| context.SwiftDemoVoice.ViewportProfileForTest(1680, 1050) !== "wide") {
	throw new Error("viewport aspect profile detection failed");
}

function takeScheduledCallback(delay) {
	for (var index = 0; index < scheduledCallbacks.length; index++) {
		if (scheduledCallbacks[index].delay !== delay) continue;
		return scheduledCallbacks.splice(index, 1)[0].callback;
	}
	return null;
}

if (startupClasses["demo-active"] !== true || startupClasses.collapsed !== false) {
	throw new Error("self-start must make the menu visible and expanded");
}
if (commands[0] !== "tv_listen_voice_indices -1" || commands[1] !== "tv_listen_voice_indices_h -1") {
	throw new Error("Demo voice must default both 32-bit slot masks to all enabled: " + JSON.stringify(commands));
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

var speakingData = {
	holdTicks: 30,
	pulsesBySlot: {
		"3": [100, 110, 200],
		"40": [115]
	}
};
var speakingAt120 = context.SwiftDemoVoice.SpeakingSlotsForTick(120, speakingData);
var speakingAt141 = context.SwiftDemoVoice.SpeakingSlotsForTick(141, speakingData);
var speakingAt200 = context.SwiftDemoVoice.SpeakingSlotsForTick(200, speakingData);
if (JSON.stringify(speakingAt120) !== JSON.stringify([3, 40])
	|| JSON.stringify(speakingAt141) !== JSON.stringify([40])
	|| JSON.stringify(speakingAt200) !== JSON.stringify([3])) {
	throw new Error("parsed voice pulse lookup failed");
}

var testRounds = [
	{ nTickStart: 0, nTickEnd: 100 },
	{ nTickStart: 100, nTickEnd: 300 },
	{ nTickStart: 300, nTickEnd: 500 }
];
if (context.SwiftDemoVoice.RoundNumberForTick(-1, testRounds) !== 0 ||
	context.SwiftDemoVoice.RoundNumberForTick(0, testRounds) !== 1 ||
	context.SwiftDemoVoice.RoundNumberForTick(99, testRounds) !== 1 ||
	context.SwiftDemoVoice.RoundNumberForTick(100, testRounds) !== 2 ||
	context.SwiftDemoVoice.RoundNumberForTick(499, testRounds) !== 3 ||
	context.SwiftDemoVoice.RoundNumberForTick(500, testRounds) !== 3) {
	throw new Error("round number lookup failed");
}

demoState = { nTick: 125, nSecondsPerTick: 0.015625, RoundIntervals: testRounds };
if (!context.SwiftDemoVoice.JumpToRound(2) || gotoTicks[0] !== 300 || context.SwiftDemoVoice.JumpToRound(8)) {
	throw new Error("direct round jump failed: " + JSON.stringify(gotoTicks));
}

var testPlayerSources = [
	{ xuid: "101", team: 1 },
	{ xuid: "202", team: 2, slot: 8, name: "Fallback Name" }
];
context.GameStateAPI.GetPlayerDataJSO = function () {
	return {
		teams: [{ name: "" }, { name: "TERRORIST" }, { name: "CT" }],
		players: testPlayerSources
	};
};
context.GameStateAPI.GetPlayerSlot = function (xuid) { return xuid === "101" ? 3 : -1; };
context.GameStateAPI.GetPlayerName = function (xuid) { return xuid === "101" ? "Primary Name" : ""; };
context.GameStateAPI.GetPlayerTeamName = function () { return ""; };
context.GameStateAPI.GetPlayerStatsJSO = function (xuid) { return { status: xuid === "202" ? 1 : 0 }; };
var nativeMutedPlayers = { "101": true, "202": true };
var nativeMuteToggles = [];
context.GameStateAPI.IsSelectedPlayerMuted = function (xuid) { return !!nativeMutedPlayers[xuid]; };
context.GameStateAPI.ToggleMute = function (xuid) {
	nativeMuteToggles.push(xuid);
	nativeMutedPlayers[xuid] = !nativeMutedPlayers[xuid];
};
var players = context.SwiftDemoVoice.ReadPlayersForTest();
if (players.length !== 2 || players[0].slot !== 3 || players[0].team !== "TERRORIST" || players[1].slot !== 8 || players[1].name !== "Fallback Name" || !players[0].canFocus || !players[1].isDead || players[1].canFocus) {
	throw new Error("demo player discovery/fallback failed: " + JSON.stringify(players));
}

var initialDemoPoll = takeScheduledCallback(0.25);
if (!initialDemoPoll) throw new Error("Demo voice startup poll was not scheduled");
initialDemoPoll();
if (JSON.stringify(nativeMuteToggles) !== JSON.stringify(["101", "202"])) {
	throw new Error("Demo startup must clear native mutes through GameStateAPI: " + JSON.stringify(nativeMuteToggles));
}

nativeMutedPlayers["101"] = true;
nativeMutedPlayers["303"] = true;
testPlayerSources.push({ xuid: "303", team: 1, slot: 12, name: "Late Player" });
var newPlayerPoll = takeScheduledCallback(0.75);
if (!newPlayerPoll) throw new Error("new-player Demo poll was not scheduled");
newPlayerPoll();
if (!nativeMutedPlayers["101"] || nativeMutedPlayers["303"] || nativeMuteToggles[nativeMuteToggles.length - 1] !== "303") {
	throw new Error("only newly discovered Demo XUIDs should be unmuted automatically");
}

function assertVisibleSpeakers(expected, label) {
	var actual = context.SwiftDemoVoice.FilterSpeakingSlotsForSelection([3, 8, 40]);
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		throw new Error(label + " speaker HUD filter failed: " + JSON.stringify(actual));
	}
}

context.SwiftDemoVoice.SelectAll();
if (nativeMutedPlayers["101"] || nativeMuteToggles[nativeMuteToggles.length - 1] !== "101") {
	throw new Error("Hear All must clear current native mutes through GameStateAPI");
}
assertVisibleSpeakers([3, 8, 40], "all voices");
context.SwiftDemoVoice.SelectNone();
assertVisibleSpeakers([], "muted voices");
context.SwiftDemoVoice.SelectTeam("TERRORIST");
assertVisibleSpeakers([3], "T only");
context.SwiftDemoVoice.SelectTeam("CT");
assertVisibleSpeakers([8], "CT only");
nativeMutedPlayers["202"] = true;
context.SwiftDemoVoice.SelectNone();
context.SwiftDemoVoice.TogglePlayer(8, "Fallback Name", "202");
if (nativeMutedPlayers["202"] || nativeMuteToggles[nativeMuteToggles.length - 1] !== "202") {
	throw new Error("enabling one Demo player must clear that XUID's native mute through GameStateAPI");
}
assertVisibleSpeakers([8], "single player");
context.SwiftDemoVoice.SelectAll();

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

console.log("demo voice mask, round navigation, native UI, and first-person POV tests passed");
