"use strict";

var SwiftDemoVoice = (function () {
	var _players = [];
	var _selectedSlots = {};
	var _selectionMode = "all";
	var _isOpen = true;
	var _wasDemo = false;
	var _lastPlayerSignature = "";
	var _lastHudPlayerXuid = "";
	var _focusGeneration = 0;
	var _started = false;

	function _Context() {
		return $.GetContextPanel();
	}

	function _Panel(id) {
		var context = _Context();
		return context ? context.FindChildTraverse(id) : null;
	}

	function _DemoController() {
		var menu = _Panel("SwiftDemoVoiceMenu");
		var panel = menu && menu.GetParent ? menu.GetParent() : _Context();
		for (var depth = 0; panel && depth < 6; depth++) {
			if (panel.IsPlayingDemo || panel.GetDemoControllerState) return panel;
			panel = panel.GetParent ? panel.GetParent() : null;
		}
		return _Context();
	}

	function _SetClass(panel, className, enabled) {
		if (!panel) return;
		if (panel.SetHasClass) {
			panel.SetHasClass(className, enabled);
			return;
		}
		if (enabled) panel.AddClass(className);
		else panel.RemoveClass(className);
	}

	function _SetText(id, text) {
		var panel = _Panel(id);
		if (panel) panel.text = text;
	}

	function _IsDemoPlayback() {
		var context = _DemoController();
		try {
			if (context && context.IsPlayingDemo && context.IsPlayingDemo()) return true;
		} catch (error) {
			$.Msg("[SwiftDemoVoice] IsPlayingDemo failed: " + error);
		}
		try {
			if (context && context.GetDemoControllerState) {
				var state = context.GetDemoControllerState();
				if (state !== null && state !== undefined) return true;
			}
		} catch (error) {
			$.Msg("[SwiftDemoVoice] GetDemoControllerState failed: " + error);
		}
		try {
			return !!GameStateAPI.IsDemoOrHltv();
		} catch (error) {
			return false;
		}
	}

	function _NormalizeSlot(slot) {
		var value = Number(slot);
		if (!isFinite(value)) return -1;
		value = Math.floor(value);
		return value >= 0 && value < 64 ? value : -1;
	}

	function BuildMasksForSlots(slots) {
		var low = 0;
		var high = 0;
		var seen = {};
		for (var i = 0; i < slots.length; i++) {
			var slot = _NormalizeSlot(slots[i]);
			if (slot < 0 || seen[slot]) continue;
			seen[slot] = true;
			if (slot < 32) {
				low = low | (1 << slot);
			} else {
				high = high | (1 << (slot - 32));
			}
		}
		return { low: low | 0, high: high | 0 };
	}

	function _RunMaskCommands(low, high, status) {
		GameInterfaceAPI.ConsoleCommand("tv_listen_voice_indices " + (low | 0));
		GameInterfaceAPI.ConsoleCommand("tv_listen_voice_indices_h " + (high | 0));
		_SetText("SwiftDemoVoiceMask", "LOW  " + (low | 0) + "     HIGH  " + (high | 0));
		if (status) _SetText("SwiftDemoVoiceStatus", status);
		$.Msg("[SwiftDemoVoice] masks low=" + (low | 0) + " high=" + (high | 0));
	}

	function _PlayerSignature(players) {
		var parts = [];
		for (var i = 0; i < players.length; i++) {
			parts.push(players[i].slot + ":" + players[i].xuid + ":" + players[i].team + ":" + players[i].name + ":" + players[i].status);
		}
		return parts.join("|");
	}

	function _TeamSortValue(team) {
		if (team === "TERRORIST") return 0;
		if (team === "CT") return 1;
		return 2;
	}

	function _TeamLabel(team) {
		if (team === "TERRORIST") return "TERRORIST";
		if (team === "CT") return "COUNTER-TERRORIST";
		return team || "SPECTATOR";
	}

	function _ResolvePlayerSlot(xuid, source) {
		var slot = -1;
		try {
			slot = _NormalizeSlot(GameStateAPI.GetPlayerSlot(xuid));
		} catch (error) {
			slot = -1;
		}
		if (slot >= 0) return slot;

		if (source) {
			slot = _NormalizeSlot(source.slot);
			if (slot < 0) slot = _NormalizeSlot(source.player_slot);
			if (slot >= 0) return slot;
		}

		try {
			var stats = GameStateAPI.GetPlayerStatsJSO(xuid);
			slot = _NormalizeSlot(stats && stats.slot);
			if (slot >= 0) return slot;
		} catch (error) {
			slot = -1;
		}

		try {
			if (GameStateAPI.GetPlayerXuidStringFromPlayerSlot) {
				for (var candidate = 0; candidate < 64; candidate++) {
					if (String(GameStateAPI.GetPlayerXuidStringFromPlayerSlot(candidate) || "") === xuid) return candidate;
				}
			}
		} catch (error) {
			return -1;
		}
		return -1;
	}

	function _NormalizePlayerStatus(value) {
		if (value === null || value === undefined || value === "") return -1;
		var status = Number(value);
		if (!isFinite(status)) return -1;
		return Math.floor(status);
	}

	function _ReadPlayerStatus(xuid, source) {
		var status = -1;
		try {
			var stats = GameStateAPI.GetPlayerStatsJSO(xuid);
			status = _NormalizePlayerStatus(stats && stats.status);
		} catch (error) {
			status = -1;
		}
		if (status >= 0) return status;

		status = _NormalizePlayerStatus(source && source.status);
		if (status >= 0) return status;

		try {
			if (GameStateAPI.GetPlayerStatus) status = _NormalizePlayerStatus(GameStateAPI.GetPlayerStatus(xuid));
		} catch (error) {
			status = -1;
		}
		return status;
	}

	function _ReadPlayers() {
		var result = [];
		var seenSlots = {};
		var data = null;
		try {
			data = GameStateAPI.GetPlayerDataJSO();
		} catch (error) {
			$.Msg("[SwiftDemoVoice] GetPlayerDataJSO failed: " + error);
		}
		if (!data || !data.players) return result;

		for (var i = 0; i < data.players.length; i++) {
			var source = data.players[i];
			var xuid = source && source.xuid ? String(source.xuid) : "";
			if (!xuid || xuid === "0") continue;

			var slot = -1;
			var name = "";
			var team = "";
			var status = -1;
			try {
				slot = _ResolvePlayerSlot(xuid, source);
				name = GameStateAPI.GetPlayerName(xuid) || "";
				team = GameStateAPI.GetPlayerTeamName(xuid) || "";
				status = _ReadPlayerStatus(xuid, source);
			} catch (error) {
				$.Msg("[SwiftDemoVoice] player API failed for " + xuid + ": " + error);
			}
			if (!name && source.name) name = String(source.name);
			if (!team && data.teams && source.team !== undefined && data.teams[source.team]) {
				team = data.teams[source.team].name || "";
			}
			if (slot < 0 || seenSlots[slot]) continue;
			seenSlots[slot] = true;
			result.push({
				xuid: xuid,
				slot: slot,
				name: name || ("Player " + (slot + 1)),
				team: team,
				status: status,
				isDead: status === 1,
				isDisconnected: status === 15,
				canFocus: status !== 1 && status !== 15
			});
		}

		result.sort(function (a, b) {
			var teamDelta = _TeamSortValue(a.team) - _TeamSortValue(b.team);
			return teamDelta !== 0 ? teamDelta : a.slot - b.slot;
		});
		return result;
	}

	function _SetRowSelected(row, selected) {
		_SetClass(row, "selected", selected);
		var audioIcon = row ? row.FindChildTraverse("SwiftDemoVoiceEnabled") : null;
		var audioLabel = row ? row.FindChildTraverse("SwiftDemoVoiceEnabledLabel") : null;
		if (audioIcon) audioIcon.SetImage(selected
			? "s2r://panorama/images/icons/ui/unmuted.vsvg"
			: "s2r://panorama/images/icons/ui/muted.vsvg");
		if (audioLabel) audioLabel.text = selected ? "ON" : "OFF";
	}

	function _GetHudPlayerXuid() {
		try {
			return String(GameStateAPI.GetHudPlayerXuid() || "");
		} catch (error) {
			return "";
		}
	}

	function _RenderObservedState() {
		for (var i = 0; i < _players.length; i++) {
			var player = _players[i];
			var row = _Panel("SwiftDemoVoicePlayer_" + player.slot);
			if (!row) continue;
			_SetClass(row, "observed", !!_lastHudPlayerXuid && player.xuid === _lastHudPlayerXuid);
		}
	}

	function _RenderSelection() {
		for (var i = 0; i < _players.length; i++) {
			var player = _players[i];
			var row = _Panel("SwiftDemoVoicePlayer_" + player.slot);
			_SetRowSelected(row, !!_selectedSlots[player.slot]);
		}
		_RenderObservedState();
	}

	function _RenderPlayers() {
		var list = _Panel("SwiftDemoVoicePlayerList");
		if (!list) return;
		list.RemoveAndDeleteChildren();

		if (_players.length === 0) {
			var empty = $.CreatePanel("Label", list, "SwiftDemoVoiceEmpty");
			empty.AddClass("swift-demo-voice__empty");
			empty.text = "Waiting for player data...";
			_SetText("SwiftDemoVoiceCount", "0 PLAYERS");
			_SetText("SwiftDemoVoiceStatus", "Waiting for demo player data...");
			return;
		}

		for (var i = 0; i < _players.length; i++) {
			var player = _players[i];
			var row = $.CreatePanel("Panel", list, "SwiftDemoVoicePlayer_" + player.slot);
			if (!row || !row.BLoadLayoutSnippet("SwiftDemoVoicePlayerRow")) {
				if (row) row.DeleteAsync(0);
				continue;
			}

			_SetClass(row, "team-t", player.team === "TERRORIST");
			_SetClass(row, "team-ct", player.team === "CT");
			_SetClass(row, "dead", player.isDead);
			_SetClass(row, "disconnected", player.isDisconnected);
			_SetClass(row, "pov-unavailable", !player.canFocus);
			row.FindChildTraverse("SwiftDemoVoiceSlot").text = String(player.slot + 1);
			row.FindChildTraverse("SwiftDemoVoicePlayerName").text = player.name;
			var meta = _TeamLabel(player.team) + "  /  SLOT " + (player.slot + 1);
			if (player.isDead) meta += "  /  DEAD";
			else if (player.isDisconnected) meta += "  /  OFFLINE";
			row.FindChildTraverse("SwiftDemoVoicePlayerMeta").text = meta;

			var povIcon = row.FindChildTraverse("SwiftDemoVoicePovIcon");
			var povLabel = row.FindChildTraverse("SwiftDemoVoicePovLabel");
			if (povIcon) povIcon.SetImage(player.canFocus
				? "s2r://panorama/images/icons/ui/watch.vsvg"
				: "s2r://panorama/images/icons/ui/elimination.vsvg");
			if (povLabel) povLabel.text = player.canFocus ? "VIEW" : (player.isDead ? "DEAD" : "N/A");

			var focusButton = row.FindChildTraverse("SwiftDemoVoiceFocus");
			if (focusButton) focusButton.enabled = player.canFocus;

			(function (playerSlot, playerName, playerXuid, canFocus, playerFocusButton, toggleButton) {
				if (playerFocusButton && canFocus) {
					playerFocusButton.SetPanelEvent("onactivate", function () {
						FocusPlayer(playerSlot, playerName, playerXuid);
					});
				}
				if (toggleButton) {
					toggleButton.SetPanelEvent("onactivate", function () {
						TogglePlayer(playerSlot, playerName);
					});
				}
			})(
				player.slot,
				player.name,
				player.xuid,
				player.canFocus,
				focusButton,
				row.FindChildTraverse("SwiftDemoVoiceToggle")
			);
			_SetRowSelected(row, !!_selectedSlots[player.slot]);
		}

		_SetText("SwiftDemoVoiceCount", _players.length + (_players.length === 1 ? " PLAYER" : " PLAYERS"));
		_SetText("SwiftDemoVoiceStatus", _players.length + " demo players ready - click a name for POV");
		_RenderObservedState();
	}

	function _PopulateAllVisibleSelections() {
		_selectedSlots = {};
		for (var i = 0; i < _players.length; i++) {
			_selectedSlots[_players[i].slot] = true;
		}
	}

	function _SelectedSlotArray() {
		var slots = [];
		for (var key in _selectedSlots) {
			if (_selectedSlots.hasOwnProperty(key) && _selectedSlots[key]) slots.push(Number(key));
		}
		return slots;
	}

	function _ApplyCustomSelection(status) {
		var masks = BuildMasksForSlots(_SelectedSlotArray());
		_RunMaskCommands(masks.low, masks.high, status);
		_RenderSelection();
	}

	function SelectAll() {
		_selectionMode = "all";
		_PopulateAllVisibleSelections();
		_RunMaskCommands(-1, -1, "All recorded voice slots are enabled");
		_RenderSelection();
	}

	function SelectNone() {
		_selectionMode = "none";
		_selectedSlots = {};
		_RunMaskCommands(0, 0, "All recorded voice slots are muted");
		_RenderSelection();
	}

	function SelectTeam(team) {
		_selectionMode = "custom";
		_selectedSlots = {};
		for (var i = 0; i < _players.length; i++) {
			if (_players[i].team === team) _selectedSlots[_players[i].slot] = true;
		}
		_ApplyCustomSelection(team === "CT" ? "Listening to Counter-Terrorists" : "Listening to Terrorists");
	}

	function TogglePlayer(slot, playerName) {
		if (_selectionMode === "all") _PopulateAllVisibleSelections();
		else if (_selectionMode === "none") _selectedSlots = {};
		_selectionMode = "custom";
		_selectedSlots[slot] = !_selectedSlots[slot];
		_ApplyCustomSelection((_selectedSlots[slot] ? "Enabled " : "Muted ") + playerName);
	}

	function _AccountIdFromXuid(xuid) {
		var text = String(xuid || "");
		var value = 0;
		if (!/^\d+$/.test(text)) return 0;
		for (var i = 0; i < text.length; i++) {
			value = (value * 10 + Number(text.charAt(i))) % 4294967296;
		}
		return value;
	}

	function _SafeConsolePlayerName(playerName) {
		return String(playerName || "")
			.replace(/[;\r\n]/g, " ")
			.replace(/[\\\"]/g, "");
	}

	function _FocusMatchesXuid(playerXuid) {
		return !!playerXuid && _GetHudPlayerXuid() === String(playerXuid);
	}

	function _FindPlayerForFocus(slot, playerXuid) {
		var targetXuid = String(playerXuid || "");
		for (var i = 0; i < _players.length; i++) {
			if (_players[i].slot === slot || (targetXuid && _players[i].xuid === targetXuid)) return _players[i];
		}
		return null;
	}

	function _SetFirstPersonMode() {
		// CS2's current DemoController enum maps OBS_MODE_IN_EYE to 2.
		GameInterfaceAPI.ConsoleCommand("spec_mode 2");
	}

	function _FinishVerifiedFocus(generation, displaySlot, playerName, playerXuid) {
		if (generation !== _focusGeneration || !_FocusMatchesXuid(playerXuid)) return false;
		_SetFirstPersonMode();
		_lastHudPlayerXuid = String(playerXuid);
		_RenderObservedState();
		_SetText("SwiftDemoVoiceStatus", "POV: " + playerName + "  /  SLOT " + displaySlot);
		$.Msg("[SwiftDemoVoice] focus verified slot=" + displaySlot + " player=" + playerName);
		return true;
	}

	function _VerifyFocus(generation, normalizedSlot, displaySlot, playerName, playerXuid, attempt) {
		if (generation !== _focusGeneration) return;
		if (_FinishVerifiedFocus(generation, displaySlot, playerName, playerXuid)) return;

		if (attempt === 0) {
			var safeName = _SafeConsolePlayerName(playerName);
			if (safeName) GameInterfaceAPI.ConsoleCommand("spec_player \"" + safeName + "\"");
			$.Schedule(0.12, function () {
				_VerifyFocus(generation, normalizedSlot, displaySlot, playerName, playerXuid, 1);
			});
			return;
		}

		if (attempt === 1) {
			GameInterfaceAPI.ConsoleCommand("spec_player " + normalizedSlot);
			$.Schedule(0.12, function () {
				_VerifyFocus(generation, normalizedSlot, displaySlot, playerName, playerXuid, 2);
			});
			return;
		}

		var currentXuid = _GetHudPlayerXuid();
		var targetStatus = _ReadPlayerStatus(playerXuid, null);
		if (targetStatus === 1 || targetStatus === 15) {
			_SetText("SwiftDemoVoiceStatus", targetStatus === 1 ? "POV unavailable: player is dead" : "POV unavailable: player is offline");
			$.Msg("[SwiftDemoVoice] focus unavailable slot=" + displaySlot + " player=" + playerName + " status=" + targetStatus);
			Refresh(false);
		} else {
			_SetText("SwiftDemoVoiceStatus", "POV switch failed: " + playerName);
			$.Msg("[SwiftDemoVoice] focus failed slot=" + displaySlot + " player=" + playerName + " current=" + currentXuid);
		}
	}

	function FocusPlayer(slot, playerName, playerXuid) {
		var normalizedSlot = _NormalizeSlot(slot);
		if (normalizedSlot < 0) return;
		var knownPlayer = _FindPlayerForFocus(normalizedSlot, playerXuid);
		if (knownPlayer && !knownPlayer.canFocus) {
			_SetText("SwiftDemoVoiceStatus", knownPlayer.isDead ? "POV unavailable: player is dead" : "POV unavailable: player is offline");
			return;
		}
		var displaySlot = normalizedSlot + 1;
		var targetXuid = String(playerXuid || "");
		var accountId = _AccountIdFromXuid(targetXuid);
		var generation = ++_focusGeneration;

		_SetText("SwiftDemoVoiceStatus", "Switching POV: " + playerName + "...");
		_SetFirstPersonMode();
		if (accountId > 0) GameInterfaceAPI.ConsoleCommand("spec_lock_to_accountid " + accountId);
		GameInterfaceAPI.ConsoleCommand("spec_player " + displaySlot);

		if (!targetXuid) {
			$.Schedule(0.08, _SetFirstPersonMode);
			$.Msg("[SwiftDemoVoice] focus requested without xuid slot=" + displaySlot + " player=" + playerName);
			return;
		}

		$.Msg("[SwiftDemoVoice] focus requested slot=" + displaySlot + " player=" + playerName + " accountid=" + accountId);
		$.Schedule(0.12, function () {
			_VerifyFocus(generation, normalizedSlot, displaySlot, playerName, targetXuid, 0);
		});
	}

	function Refresh(forceRender) {
		var players = _ReadPlayers();
		var signature = _PlayerSignature(players);
		var changed = signature !== _lastPlayerSignature;
		_players = players;
		_lastPlayerSignature = signature;

		if (_selectionMode === "all") _PopulateAllVisibleSelections();
		else if (_selectionMode === "none") _selectedSlots = {};

		if (changed || forceRender) _RenderPlayers();
		else _RenderSelection();
		if (forceRender) _SetText("SwiftDemoVoiceStatus", "Player list refreshed");
	}

	function _SetOpen(open) {
		_isOpen = !!open;
		var menu = _Panel("SwiftDemoVoiceMenu");
		_SetClass(menu, "collapsed", !_isOpen);
	}

	function ToggleOpen() {
		_SetOpen(!_isOpen);
	}

	function _Poll() {
		var isDemo = _IsDemoPlayback();
		var menu = _Panel("SwiftDemoVoiceMenu");
		// CSGOHudDemoController itself is hidden by the engine outside demo playback.
		// Keep this child visible and refresh data even if one demo-state API lags.
		_SetClass(menu, "demo-active", true);
		_lastHudPlayerXuid = isDemo ? _GetHudPlayerXuid() : "";
		Refresh(false);

		if (isDemo && !_wasDemo) {
			_wasDemo = true;
			_SetOpen(true);
			Refresh(true);
			SelectAll();
			$.Msg("[SwiftDemoVoice] demo detected; voice slots enabled");
		} else if (!isDemo && _wasDemo) {
			_wasDemo = false;
			_lastPlayerSignature = "";
		}

		$.Schedule(0.75, _Poll);
	}

	function OnLoad() {
		if (_started) return;
		_started = true;
		_SetClass(_Panel("SwiftDemoVoiceMenu"), "demo-active", true);
		_SetOpen(true);
		Refresh(true);
		_RunMaskCommands(-1, -1, "Loading demo players...");
		$.Schedule(0.25, _Poll);
		$.Msg("[SwiftDemoVoice] runtime loaded");
	}
	return {
		BuildMasksForSlots: BuildMasksForSlots,
		AccountIdFromXuid: _AccountIdFromXuid,
		ReadPlayersForTest: _ReadPlayers,
		SelectAll: SelectAll,
		SelectNone: SelectNone,
		SelectTeam: SelectTeam,
		TogglePlayer: TogglePlayer,
		FocusPlayer: FocusPlayer,
		Refresh: Refresh,
		ToggleOpen: ToggleOpen,
		OnLoad: OnLoad
	};
})();

// Some CS2 builds do not dispatch XML Panel onload consistently for panels that
// start collapsed. Boot from the included script as well; OnLoad is idempotent.
$.Schedule(0.0, SwiftDemoVoice.OnLoad);
