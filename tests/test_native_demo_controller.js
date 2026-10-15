"use strict";

// Run against a current GameTracking-CS2 checkout. No game process is started.
var assert = require("assert");
var fs = require("fs");
var path = require("path");
var vm = require("vm");
var trackingRoot = process.argv[2];
if (!trackingRoot) throw new Error("Usage: node tests/test_native_demo_controller.js <GameTracking-CS2>");
var panorama = path.join(trackingRoot, "game/csgo/pak01_dir/panorama");
var layout = fs.readFileSync(path.join(__dirname, "../addon/panorama/layout/hud/huddemocontroller.xml"), "utf8");
var nativeLayout = fs.readFileSync(path.join(panorama, "layout/hud/huddemocontroller.xml"), "utf8");
var nativeSource = fs.readFileSync(path.join(panorama, "scripts/hud/huddemocontroller.js"), "utf8");

function nativeRoot(xml) {
    var start = xml.indexOf('<Panel id="Root">');
    assert(start >= 0);
    var pattern = /<\/?Panel\b[^>]*>/g;
    pattern.lastIndex = start;
    var depth = 0, token;
    while ((token = pattern.exec(xml))) {
        if (/^<\/Panel/.test(token[0])) depth--;
        else if (!/\/>$/.test(token[0])) depth++;
        if (!depth) return xml.slice(start, pattern.lastIndex).replace(/\r\n/g, "\n");
    }
    throw new Error("Incomplete native Root");
}
assert.strictEqual(nativeRoot(layout), nativeRoot(nativeLayout), "override must preserve the current Valve Root");

function panel(id) {
    var classes = new Set();
    return {
        id: id, style: {}, visible: true, actuallayoutwidth: 100, actualuiscale_x: 1,
        BHasClass: function (name) { return classes.has(name); },
        SetHasClass: function (name, enabled) { if (enabled) classes.add(name); else classes.delete(name); },
        AddClass: function (name) { classes.add(name); },
        RemoveClass: function (name) { classes.delete(name); },
        ToggleClass: function (name) { this.SetHasClass(name, !this.BHasClass(name)); },
        SetDialogVariable: function () {}, SetDialogVariableInt: function () {},
        SetLocString: function () {}, SetPanelEvent: function () {},
        SetValueNoEvents: function (value) { this.value = value; },
        SetSelected: function () {}, RemoveAndDeleteChildren: function () {},
        GetPositionWithinAncestor: function () { return { x: 0, y: 0 }; },
        FindChildTraverse: function (name) { return name === "SliderThumb" ? thumb : null; },
        BReadyForDisplay: function () { return true; }, IsSizeValid: function () { return true; }
    };
}
var panels = {};
for (var match of layout.matchAll(/\bid="([^"]+)"/g)) panels[match[1]] = panel(match[1]);
var thumb = panel("SliderThumb"); // Slider creates this child at runtime.
panels.SliderThumb = thumb;
var hud = panel("Hud");
var cp = panel("DemoController");
var state = null;
var commands = [], jumps = [], scheduled = [], handlers = {};
cp.GetParent = function () { return hud; };
cp.IsPlayingDemo = function () { return true; };
cp.GetDemoControllerState = function () { return state; };
cp.GetDemoPlayers = function () { return []; };
cp.GetActionStartTicks = function () { return [120]; };
cp.SetHighlightsModeEnabled = function () {};
cp.GotoTick = function (tick) { jumps.push(tick); };
cp.SetPaused = function (paused) { state.bIsPaused = paused; };
cp.SetDemoTimeScale = function (speed) { state.fTimeScale = speed; };
var $ = function (selector) { return panels[selector.slice(1)] || null; };
$.GetContextPanel = function () { return cp; };
$.Schedule = function (_, callback) { scheduled.push(callback); };
$.Localize = function (token) { return token; };
$.CreatePanel = function (_, parent, id) { assert(parent); return panel(id); };
$.RegisterEventHandler = function (name, target, callback) {
    assert(target, "native event target missing: " + name);
    handlers[target.id + ":" + name] = callback;
};
$.RegisterForUnhandledEvent = function () {};
var context = {
    $: $, UiToolkitAPI: { ShowTextTooltip: function () {} },
    GameInterfaceAPI: {
        GetSettingString: function (name) { return name === "demo_ui_mode" ? "2" : "0"; },
        ConsoleCommand: function (command) { commands.push(command); }
    }
};
vm.runInNewContext(nativeSource, context, { filename: "Valve/huddemocontroller.js" });
assert.strictEqual(panels.SpeedSlider.max, 6, "native speed slider must initialize");
var frame = scheduled.shift();
assert.strictEqual(typeof frame, "function");
frame(); // Before a Demo has loaded.
scheduled.length = 0;
state = {
    sFileName: "test.dem", nTotalTicks: 1000, nTick: 130, nSecondsPerTick: 1 / 64,
    RoundIntervals: [{ nTickStart: 0, nTickEnd: 1000 }],
    nSpectatingPlayerId: -1, nObserverMode: 2, bIsPlayingHighlights: false,
    bIsPaused: false, fTimeScale: 1
};
frame();
assert(hud.BHasClass("DemoControllerFull"), "native frame must enable the class that shows Demo Voice");
assert.strictEqual(panels.TimeScale.text, "1x");
handlers["SpeedSlider:SliderValueChanged"](null, 5);
assert.strictEqual(state.fTimeScale, 2);
context.HudDemoController.OnPlayClicked();
assert.strictEqual(state.bIsPaused, true);
context.HudDemoController.OnPlayClicked();
assert.strictEqual(state.bIsPaused, false);
context.HudDemoController.TogglePlayerListVisible();
assert(panels.PlayerList.BHasClass("Visible"));
context.HudDemoController.OnRoundButtonActivate();
assert(panels.RoundMenu.BHasClass("Visible"));
assert(!panels.PlayerList.BHasClass("Visible"));
context.HudDemoController.OnStepInterval(0);
assert.deepStrictEqual(jumps, [0]);
context.HudDemoController.OnNextCamera();
assert.deepStrictEqual(commands, ["spec_mode"]);
context.HudDemoController.ToggleSettingsVisible();
assert(!panels.RoundMenu.BHasClass("Visible"));
frame();
console.log("current Valve DemoUI layout, startup, visibility, playback, player/round menus, and settings passed");
