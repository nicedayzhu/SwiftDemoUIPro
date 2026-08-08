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
	var _rounds = [];
	var _roundMenuOpen = false;
	var _lastRoundSignature = "";
	var _currentRound = 0;
	var _started = false;
	var _lastSpeakingSignature = "";
	var _lastVoiceStatusSignature = "";
	var _voiceDataLogged = false;

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

	function _Localize(token, variables) {
		var panel = _Panel("SwiftDemoVoiceMenu") || _Context();
		if (variables && panel) {
			for (var name in variables) {
				if (!variables.hasOwnProperty(name)) continue;
				var value = variables[name];
				if (typeof value === "number" && panel.SetDialogVariableInt) {
					panel.SetDialogVariableInt(name, Math.floor(value));
				} else if (panel.SetDialogVariable) {
					panel.SetDialogVariable(name, String(value));
				}
			}
		}
		return $.Localize ? $.Localize(token, panel) : token;
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

	function _GetDemoState() {
		var context = _DemoController();
		try {
			return context && context.GetDemoControllerState ? context.GetDemoControllerState() : null;
		} catch (error) {
			$.Msg("[SwiftDemoVoice] GetDemoControllerState failed: " + error);
			return null;
		}
	}

	function _GetVoiceData() {
		if (typeof SwiftDemoVoiceData === "undefined" || !SwiftDemoVoiceData) return null;
		if (Number(SwiftDemoVoiceData.schemaVersion) !== 1 || !SwiftDemoVoiceData.pulsesBySlot) return null;
		return SwiftDemoVoiceData;
	}

	function _LatestPulseAtOrBefore(ticks, tick) {
		if (!ticks || ticks.length === 0) return -1;
		var low = 0;
		var high = ticks.length - 1;
		var result = -1;
		while (low <= high) {
			var middle = Math.floor((low + high) / 2);
			if (Number(ticks[middle]) <= tick) {
				result = middle;
				low = middle + 1;
			} else {
				high = middle - 1;
			}
		}
		return result;
	}

	function _SpeakingSlotsForTick(tick, data) {
		var result = [];
		if (!data || !data.pulsesBySlot || !isFinite(Number(tick))) return result;
		var currentTick = Math.floor(Number(tick));
		var holdTicks = Math.max(1, Math.floor(Number(data.holdTicks || 30)));
		for (var slotText in data.pulsesBySlot) {
			if (!data.pulsesBySlot.hasOwnProperty(slotText)) continue;
			var slot = _NormalizeSlot(slotText);
			if (slot < 0) continue;
			var ticks = data.pulsesBySlot[slotText];
			var pulseIndex = _LatestPulseAtOrBefore(ticks, currentTick);
			if (pulseIndex < 0) continue;
			var age = currentTick - Number(ticks[pulseIndex]);
			if (age >= 0 && age <= holdTicks) result.push(slot);
		}
		result.sort(function (left, right) { return left - right; });
		return result;
	}

	function _PlayerBySlot(slot) {
		for (var index = 0; index < _players.length; index++) {
			if (_players[index].slot === slot) return _players[index];
		}
		return null;
	}

	function _SpeakingPlayerForSlot(slot) {
		var player = _PlayerBySlot(slot);
		if (player) return player;
		return {
			slot: slot,
			xuid: "",
			name: _Localize("#SwiftDemoVoice_UnknownSpeaker", { slot: slot + 1 }),
			team: "",
			status: 0,
			isDead: false
		};
	}

	function _UpdateVoiceIndexStatus(data, state, slots, errorText) {
		var status = _Panel("SwiftDemoVoiceIndexStatus");
		if (!status) return;
		var generated = !!(data && data.generated);
		var tick = state && isFinite(Number(state.nTick)) ? Math.floor(Number(state.nTick)) : -1;
		var active = [];
		for (var index = 0; index < slots.length; index++) active.push(slots[index] + 1);
		var signature = generated + ":" + Number(data && data.voicePacketCount || 0) + ":" + tick + ":" + active.join(",") + ":" + String(errorText || "");
		if (signature === _lastVoiceStatusSignature) return;
		_lastVoiceStatusSignature = signature;
		_SetClass(status, "ready", generated && !errorText);
		_SetClass(status, "active", generated && !errorText && active.length > 0);
		if (errorText) {
			status.text = _Localize("#SwiftDemoVoice_IndexError", { error: errorText });
		} else if (!generated) {
			status.text = _Localize("#SwiftDemoVoice_IndexFallback");
		} else {
			status.text = _Localize("#SwiftDemoVoice_IndexReady", {
				packets: Number(data.voicePacketCount || 0),
				tick: tick,
				active: active.length > 0 ? active.join(",") : "-"
			});
		}
	}

	function _RenderSpeakingPlayers(slots) {
		var list = _Panel("SwiftDemoVoiceNoticeList");
		if (!list) return;
		var signatureParts = [];
		for (var index = 0; index < slots.length; index++) {
			var known = _SpeakingPlayerForSlot(slots[index]);
			signatureParts.push(known.slot + ":" + known.xuid + ":" + known.name + ":" + known.status);
		}
		var signature = signatureParts.join("|");
		if (signature === _lastSpeakingSignature) return;
		_lastSpeakingSignature = signature;
		list.RemoveAndDeleteChildren();

		for (var slotIndex = 0; slotIndex < slots.length; slotIndex++) {
			var player = _SpeakingPlayerForSlot(slots[slotIndex]);
			var notice = $.CreatePanel("Panel", list, "SwiftDemoSpeakingPlayer_" + player.slot);
			if (!notice || !notice.BLoadLayoutSnippet("SwiftDemoSpeakingPlayer")) {
				if (notice) notice.DeleteAsync(0);
				continue;
			}
			_SetClass(notice, "team-t", player.team === "TERRORIST");
			_SetClass(notice, "team-ct", player.team === "CT");
			_SetClass(notice, "dead", player.isDead);
			var name = notice.FindChildTraverse("SwiftDemoSpeakingName");
			if (name) name.text = player.name;
			var avatar = notice.FindChildTraverse("SwiftDemoSpeakingAvatar");
			if (player.xuid && avatar && avatar.PopulateFromSteamID) {
				try {
					avatar.PopulateFromSteamID(player.xuid);
				} catch (error) {
					$.Msg("[SwiftDemoVoice] unable to populate speaker avatar: " + error);
				}
			}
		}
	}

	function _PollVoiceActivity() {
		$.Schedule(0.05, _PollVoiceActivity);
		var data = null;
		var state = null;
		var slots = [];
		try {
			data = _GetVoiceData();
			state = _GetDemoState();
			slots = data && state ? _SpeakingSlotsForTick(state.nTick, data) : [];
			_RenderSpeakingPlayers(slots);
			_UpdateVoiceIndexStatus(data, state, slots, "");
			if (data && !_voiceDataLogged) {
				_voiceDataLogged = true;
				$.Msg("[SwiftDemoVoice] parsed voice index loaded generated=" + !!data.generated + " packets=" + Number(data.voicePacketCount || 0));
			}
		} catch (error) {
			_UpdateVoiceIndexStatus(data, state, slots, String(error));
			$.Msg("[SwiftDemoVoice] voice activity poll failed: " + error);
		}
	}

	function _RoundNumberForTick(tick, rounds) {
		if (!rounds || rounds.length === 0 || rounds[0].nTickStart > tick) return 0;
		for (var i = 0; i < rounds.length; i++) {
			if (tick < rounds[i].nTickStart) return i;
		}
		return rounds.length;
	}

	function _FormatTickTime(tick, secondsPerTick) {
		var totalSeconds = Math.max(0, Math.floor(Number(tick || 0) * Number(secondsPerTick || 0)));
		var minutes = Math.floor(totalSeconds / 60);
		var seconds = totalSeconds - minutes * 60;
		return minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
	}

	function _RoundSignature(rounds, secondsPerTick) {
		var parts = [String(rounds.length), String(secondsPerTick || 0)];
		for (var i = 0; i < rounds.length; i++) {
			parts.push(rounds[i].nTickStart + ":" + rounds[i].nTickEnd);
		}
		return parts.join("|");
	}

	function _SetRoundPickerOpen(open) {
		_roundMenuOpen = !!open;
		_SetClass(_Panel("SwiftDemoRoundPicker"), "open", _roundMenuOpen);
	}

	function ToggleRoundPicker() {
		if (_rounds.length === 0) {
			_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_RoundDataUnavailable"));
			return;
		}
		_SetRoundPickerOpen(!_roundMenuOpen);
	}

	function _RenderRoundRows(state, currentRound) {
		var list = _Panel("SwiftDemoRoundList");
		if (!list) return;
		list.RemoveAndDeleteChildren();

		var rounds = state && state.RoundIntervals ? state.RoundIntervals : [];
		var secondsPerTick = state ? Number(state.nSecondsPerTick || 0) : 0;
		for (var i = 0; i < rounds.length; i++) {
			var roundNumber = i + 1;
			var interval = rounds[i];
			var row = $.CreatePanel("Button", list, "SwiftDemoRound_" + roundNumber);
			row.AddClass("swift-demo-round-picker__round");
			_SetClass(row, "current", roundNumber === currentRound);

			var number = $.CreatePanel("Label", row, "");
			number.AddClass("swift-demo-round-picker__round-number");
			number.text = String(roundNumber);

			var copy = $.CreatePanel("Panel", row, "");
			copy.AddClass("swift-demo-round-picker__round-copy");
			var title = $.CreatePanel("Label", copy, "");
			title.AddClass("swift-demo-round-picker__round-title");
			title.text = _Localize("#SwiftDemoVoice_RoundTitle", { round: roundNumber });
			var time = $.CreatePanel("Label", copy, "");
			time.AddClass("swift-demo-round-picker__round-time");
			time.text = _FormatTickTime(interval.nTickStart, secondsPerTick) + " - " + _FormatTickTime(interval.nTickEnd, secondsPerTick);

			var marker = $.CreatePanel("Label", row, "");
			marker.AddClass("swift-demo-round-picker__round-marker");
			marker.text = _Localize(roundNumber === currentRound
				? "#SwiftDemoVoice_RoundNow"
				: "#SwiftDemoVoice_RoundGo");

			(function (roundIndex) {
				row.SetPanelEvent("onactivate", function () {
					JumpToRound(roundIndex);
				});
			})(i);
		}
	}

	function _UpdateRoundPicker(forceRender) {
		var state = _GetDemoState();
		var rounds = state && state.RoundIntervals ? state.RoundIntervals : [];
		var signature = _RoundSignature(rounds, state ? state.nSecondsPerTick : 0);
		var currentRound = state ? _RoundNumberForTick(Number(state.nTick || 0), rounds) : 0;
		var changed = signature !== _lastRoundSignature || currentRound !== _currentRound;

		_rounds = rounds;
		_lastRoundSignature = signature;
		_currentRound = currentRound;

		var toggle = _Panel("SwiftDemoRoundToggle");
		if (toggle) toggle.enabled = rounds.length > 0;
		_SetClass(_Panel("SwiftDemoRoundPicker"), "unavailable", rounds.length === 0);

		if (rounds.length === 0) {
			_SetText("SwiftDemoRoundSummary", _Localize("#SwiftDemoVoice_WaitingRoundData"));
			_SetText("SwiftDemoRoundCount", _Localize("#SwiftDemoVoice_RoundsZero"));
			_SetRoundPickerOpen(false);
			if (forceRender || changed) _RenderRoundRows(null, 0);
			return;
		}

		_SetText("SwiftDemoRoundSummary", _Localize(
			currentRound > 0 ? "#SwiftDemoVoice_RoundSummary" : "#SwiftDemoVoice_BeforeFirstRound",
			{ current: currentRound, total: rounds.length }
		));
		_SetText("SwiftDemoRoundCount", _Localize(
			rounds.length === 1 ? "#SwiftDemoVoice_RoundCountOne" : "#SwiftDemoVoice_RoundCountMany",
			{ count: rounds.length }
		));
		if (forceRender || changed) _RenderRoundRows(state, currentRound);
	}

	function JumpToRound(roundIndex) {
		var index = Math.floor(Number(roundIndex));
		var state = _GetDemoState();
		var rounds = state && state.RoundIntervals ? state.RoundIntervals : [];
		var controller = _DemoController();
		if (!isFinite(index) || index < 0 || index >= rounds.length || !controller || !controller.GotoTick) {
			_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_JumpRoundUnavailable"));
			return false;
		}

		var targetRound = index + 1;
		controller.GotoTick(Math.floor(rounds[index].nTickStart));
		_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_JumpingRound", { round: targetRound }));
		_SetText("SwiftDemoRoundSummary", _Localize("#SwiftDemoVoice_RoundSummary", {
			current: targetRound,
			total: rounds.length
		}));
		_SetRoundPickerOpen(false);
		$.Schedule(0.12, function () { _UpdateRoundPicker(true); });
		$.Msg("[SwiftDemoVoice] jump to round=" + targetRound + " tick=" + rounds[index].nTickStart);
		return true;
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
		_SetText("SwiftDemoVoiceMask", _Localize("#SwiftDemoVoice_VoiceMask", {
			low: low | 0,
			high: high | 0
		}));
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
		if (team === "TERRORIST") return _Localize("#SwiftDemoVoice_TeamTerrorist");
		if (team === "CT") return _Localize("#SwiftDemoVoice_TeamCounterTerrorist");
		return team || _Localize("#SwiftDemoVoice_TeamSpectator");
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
				name: name || _Localize("#SwiftDemoVoice_PlayerFallback", { slot: slot + 1 }),
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
		if (audioLabel) audioLabel.text = _Localize(selected
			? "#SwiftDemoVoice_On"
			: "#SwiftDemoVoice_Off");
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
			empty.text = _Localize("#SwiftDemoVoice_WaitingPlayerData");
			_SetText("SwiftDemoVoiceCount", _Localize("#SwiftDemoVoice_PlayersZero"));
			_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_WaitingDemoPlayerData"));
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
			var playerStatus = player.isDead
				? _Localize("#SwiftDemoVoice_Dead")
				: (player.isDisconnected ? _Localize("#SwiftDemoVoice_Offline") : "");
			var meta = _Localize(
				playerStatus ? "#SwiftDemoVoice_PlayerMetaStatus" : "#SwiftDemoVoice_PlayerMeta",
				{ team: _TeamLabel(player.team), slot: player.slot + 1, status: playerStatus }
			);
			row.FindChildTraverse("SwiftDemoVoicePlayerMeta").text = meta;

			var povIcon = row.FindChildTraverse("SwiftDemoVoicePovIcon");
			var povLabel = row.FindChildTraverse("SwiftDemoVoicePovLabel");
			if (povIcon) povIcon.SetImage(player.canFocus
				? "s2r://panorama/images/icons/ui/watch.vsvg"
				: "s2r://panorama/images/icons/ui/elimination.vsvg");
			if (povLabel) povLabel.text = _Localize(player.canFocus
				? "#SwiftDemoVoice_View"
				: (player.isDead ? "#SwiftDemoVoice_Dead" : "#SwiftDemoVoice_NotAvailable"));

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

		_SetText("SwiftDemoVoiceCount", _Localize(
			_players.length === 1 ? "#SwiftDemoVoice_PlayerCountOne" : "#SwiftDemoVoice_PlayerCountMany",
			{ count: _players.length }
		));
		_SetText("SwiftDemoVoiceStatus", _Localize(
			_players.length === 1 ? "#SwiftDemoVoice_PlayersReadyOne" : "#SwiftDemoVoice_PlayersReadyMany",
			{ count: _players.length }
		));
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
		_RunMaskCommands(-1, -1, _Localize("#SwiftDemoVoice_AllVoicesEnabled"));
		_RenderSelection();
	}

	function SelectNone() {
		_selectionMode = "none";
		_selectedSlots = {};
		_RunMaskCommands(0, 0, _Localize("#SwiftDemoVoice_AllVoicesMuted"));
		_RenderSelection();
	}

	function SelectTeam(team) {
		_selectionMode = "custom";
		_selectedSlots = {};
		for (var i = 0; i < _players.length; i++) {
			if (_players[i].team === team) _selectedSlots[_players[i].slot] = true;
		}
		_ApplyCustomSelection(_Localize(team === "CT"
			? "#SwiftDemoVoice_ListeningCounterTerrorists"
			: "#SwiftDemoVoice_ListeningTerrorists"));
	}

	function TogglePlayer(slot, playerName) {
		if (_selectionMode === "all") _PopulateAllVisibleSelections();
		else if (_selectionMode === "none") _selectedSlots = {};
		_selectionMode = "custom";
		_selectedSlots[slot] = !_selectedSlots[slot];
		_ApplyCustomSelection(_Localize(
			_selectedSlots[slot] ? "#SwiftDemoVoice_EnabledPlayer" : "#SwiftDemoVoice_MutedPlayer",
			{ player: playerName }
		));
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
		_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_PovActive", {
			player: playerName,
			slot: displaySlot
		}));
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
			_SetText("SwiftDemoVoiceStatus", _Localize(targetStatus === 1
				? "#SwiftDemoVoice_PovUnavailableDead"
				: "#SwiftDemoVoice_PovUnavailableOffline"));
			$.Msg("[SwiftDemoVoice] focus unavailable slot=" + displaySlot + " player=" + playerName + " status=" + targetStatus);
			Refresh(false);
		} else {
			_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_PovSwitchFailed", { player: playerName }));
			$.Msg("[SwiftDemoVoice] focus failed slot=" + displaySlot + " player=" + playerName + " current=" + currentXuid);
		}
	}

	function FocusPlayer(slot, playerName, playerXuid) {
		var normalizedSlot = _NormalizeSlot(slot);
		if (normalizedSlot < 0) return;
		var knownPlayer = _FindPlayerForFocus(normalizedSlot, playerXuid);
		if (knownPlayer && !knownPlayer.canFocus) {
			_SetText("SwiftDemoVoiceStatus", _Localize(knownPlayer.isDead
				? "#SwiftDemoVoice_PovUnavailableDead"
				: "#SwiftDemoVoice_PovUnavailableOffline"));
			return;
		}
		var displaySlot = normalizedSlot + 1;
		var targetXuid = String(playerXuid || "");
		var accountId = _AccountIdFromXuid(targetXuid);
		var generation = ++_focusGeneration;

		_SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_SwitchingPov", { player: playerName }));
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
		if (forceRender) _SetText("SwiftDemoVoiceStatus", _Localize("#SwiftDemoVoice_PlayerListRefreshed"));
	}

	function _SetOpen(open) {
		_isOpen = !!open;
		var menu = _Panel("SwiftDemoVoiceMenu");
		_SetClass(menu, "collapsed", !_isOpen);
		if (!_isOpen) _SetRoundPickerOpen(false);
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
		_UpdateRoundPicker(false);

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
		_UpdateRoundPicker(true);
		_RunMaskCommands(-1, -1, _Localize("#SwiftDemoVoice_LoadingPlayers"));
		$.Schedule(0.25, _Poll);
		$.Schedule(0.05, _PollVoiceActivity);
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
		RoundNumberForTick: _RoundNumberForTick,
		SpeakingSlotsForTick: _SpeakingSlotsForTick,
		JumpToRound: JumpToRound,
		ToggleRoundPicker: ToggleRoundPicker,
		Refresh: Refresh,
		ToggleOpen: ToggleOpen,
		OnLoad: OnLoad
	};
})();

// Some CS2 builds do not dispatch XML Panel onload consistently for panels that
// start collapsed. Boot from the included script as well; OnLoad is idempotent.
$.Schedule(0.0, SwiftDemoVoice.OnLoad);
