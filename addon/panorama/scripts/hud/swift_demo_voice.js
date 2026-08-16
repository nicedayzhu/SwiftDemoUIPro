"use strict";

var SwiftDemoVoice = (function () {
	var _players = [];
	var _selectedSlots = {};
	var _selectionMode = "all";
	var _showPlayerAvatars = false;
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
	var _isMenuVisible = true;
	var _nativeMenuToggleMounted = false;
	var _lastSpeakingSignature = "";
	var _lastVoiceStatusSignature = "";
	var _voiceDataLogged = false;
	var _nativeUnmuteSeenPlayers = {};
	var _lastViewportProfile = "";
	var _lastViewportWidth = 0;
	var _lastViewportHeight = 0;
	var _dragEventsRegistered = false;
	var _customPosition = false;
	var _customPositionX = 0;
	var _customPositionY = 0;
	var _customPositionRatioX = 0;
	var _customPositionRatioY = 0;
	var _panelWidth = 360;
	var _panelHeight = 650;
	var _compactWidth = 360;
	var _compactHeight = 56;
	var _screenPadding = 12;
	var _snapDistance = 24;

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

	function _ViewportProfile(width, height) {
		var viewportWidth = Number(width);
		var viewportHeight = Number(height);
		if (!(viewportWidth > 0) || !(viewportHeight > 0)) return "";
		return viewportWidth / viewportHeight <= 1.5 ? "4x3" : "wide";
	}

	function _ViewportMetrics() {
		var context = _Context();
		if (!context) return null;
		var scaleX = Number(context.actualuiscale_x) || 1;
		var scaleY = Number(context.actualuiscale_y) || 1;
		var width = Number(context.actuallayoutwidth) / scaleX;
		var height = Number(context.actuallayoutheight) / scaleY;
		if (!(width > 0) || !(height > 0)) return null;
		return {
			context: context,
			scaleX: scaleX,
			scaleY: scaleY,
			width: width,
			height: height
		};
	}

	function _CurrentPanelSize() {
		return {
			width: _isOpen ? _panelWidth : _compactWidth,
			height: _isOpen ? _panelHeight : _compactHeight
		};
	}

	function _Clamp(value, minimum, maximum) {
		return Math.max(minimum, Math.min(maximum, Number(value) || 0));
	}

	function _ResolvePosition(x, y, viewportWidth, viewportHeight, panelWidth, panelHeight, snap) {
		var minimumX = _screenPadding;
		var minimumY = _screenPadding;
		var maximumX = Math.max(minimumX, Number(viewportWidth) - Number(panelWidth) - _screenPadding);
		var maximumY = Math.max(minimumY, Number(viewportHeight) - Number(panelHeight) - _screenPadding);
		var resolvedX = _Clamp(x, minimumX, maximumX);
		var resolvedY = _Clamp(y, minimumY, maximumY);

		if (snap) {
			if (resolvedX - minimumX <= _snapDistance) resolvedX = minimumX;
			else if (maximumX - resolvedX <= _snapDistance) resolvedX = maximumX;
			if (resolvedY - minimumY <= _snapDistance) resolvedY = minimumY;
			else if (maximumY - resolvedY <= _snapDistance) resolvedY = maximumY;
		}

		return { x: Math.round(resolvedX), y: Math.round(resolvedY) };
	}

	function _ApplyCustomPosition(x, y, snap) {
		var dock = _Panel("SwiftDemoVoiceDock");
		var menu = _Panel("SwiftDemoVoiceMenu");
		var metrics = _ViewportMetrics();
		if (!dock || !metrics) return false;
		var size = _CurrentPanelSize();
		var resolved = _ResolvePosition(x, y, metrics.width, metrics.height, size.width, size.height, snap);

		_customPosition = true;
		_customPositionX = resolved.x;
		_customPositionY = resolved.y;
		_customPositionRatioX = (resolved.x + size.width * 0.5) / metrics.width;
		_customPositionRatioY = (resolved.y + size.height * 0.5) / metrics.height;
		_SetClass(dock, "custom-position", true);
		_SetClass(menu, "custom-position", true);
		dock.style.position = resolved.x + "px " + resolved.y + "px 0px";
		return true;
	}

	function _ClampCustomPosition() {
		if (!_customPosition) return;
		_ApplyCustomPosition(_customPositionX, _customPositionY, false);
	}

	function _ReflowCustomPosition() {
		if (!_customPosition) return;
		var metrics = _ViewportMetrics();
		if (!metrics) return;
		var size = _CurrentPanelSize();
		_ApplyCustomPosition(
			_customPositionRatioX * metrics.width - size.width * 0.5,
			_customPositionRatioY * metrics.height - size.height * 0.5,
			false
		);
	}

	function _DeleteDragGhost(ghost) {
		if (!ghost) return;
		try {
			// `visible` is a hard Panorama render gate. Use it before deferred
			// deletion so no drag image can survive into the source reveal frame.
			ghost.visible = false;
			ghost.style.transitionDuration = "0.0s";
			ghost.style.opacity = "0";
			if (!ghost.IsValid || ghost.IsValid()) ghost.DeleteAsync(0);
		} catch (error) {
			$.Msg("[SwiftDemoVoice] drag ghost cleanup failed: " + error);
		}
	}

	function _SetDragSourceVisible(visible) {
		var menu = _Panel("SwiftDemoVoiceMenu");
		if (!menu) return;
		// Do not use opacity here. Native drag handling can retain an
		// interpolated source frame; Panel.visible cannot be interpolated.
		menu.visible = !!visible;
	}

	function _BeginDrag(targetId, drag) {
		if (!drag || !$.CreatePanel) return false;
		var context = _Context();
		var dock = _Panel("SwiftDemoVoiceDock");
		if (!context || !dock) return false;

		// Hard-hide the source before the preview panel exists. This avoids
		// Panorama's normal drag-source treatment, which intentionally leaves a
		// dimmed copy behind for item drag-and-drop interactions.
		_SetClass(dock, "dragging", true);
		_SetDragSourceVisible(false);
		_DeleteDragGhost(_Panel("SwiftDemoVoiceDragGhost"));
		var ghost = $.CreatePanel("Panel", context, "SwiftDemoVoiceDragGhost");
		ghost.AddClass("swift-demo-voice-drag-ghost");
		if (!_isOpen) ghost.AddClass("compact");
		var header = $.CreatePanel("Panel", ghost, "SwiftDemoVoiceDragGhostHeader");
		header.AddClass("swift-demo-voice-drag-ghost__header");
		var icon = $.CreatePanel("Panel", header, "SwiftDemoVoiceDragGhostIcon");
		icon.AddClass("swift-demo-voice-drag-ghost__icon");
		var label = $.CreatePanel("Label", header, "SwiftDemoVoiceDragGhostLabel");
		label.AddClass("swift-demo-voice-drag-ghost__label");
		label.text = _Localize("#SwiftDemoVoice_Title");
		var body = $.CreatePanel("Panel", ghost, "SwiftDemoVoiceDragGhostBody");
		body.AddClass("swift-demo-voice-drag-ghost__body");
		var target = $.CreatePanel("Label", body, "SwiftDemoVoiceDragGhostTarget");
		target.AddClass("swift-demo-voice-drag-ghost__target");
		target.text = _Localize("#SwiftDemoVoice_DragPreview");

		drag.displayPanel = ghost;
		drag.offsetX = 26;
		drag.offsetY = 27;
		drag.removePositionBeforeDrop = false;
		return true;
	}

	function _EndDrag(targetId, ghost) {
		var dock = _Panel("SwiftDemoVoiceDock");
		var metrics = _ViewportMetrics();
		if (ghost && metrics) {
			var contextX = Number(metrics.context.actualxoffset) || 0;
			var contextY = Number(metrics.context.actualyoffset) || 0;
			var x = (Number(ghost.actualxoffset) - contextX) / metrics.scaleX;
			var y = (Number(ghost.actualyoffset) - contextY) / metrics.scaleY;
			if (_ApplyCustomPosition(x, y, true)) {
				$.Msg("[SwiftDemoVoice] panel moved x=" + _customPositionX + " y=" + _customPositionY);
			}
		}
		// Hard-hide the preview before restoring the real panel at its resolved
		// position. At no render point are both panels visible.
		_DeleteDragGhost(ghost);
		_SetClass(dock, "dragging", false);
		_SetDragSourceVisible(true);
	}

	function _SetupDragging() {
		if (_dragEventsRegistered || !$.RegisterEventHandler) return;
		var handle = _Panel("SwiftDemoVoiceDragHandle");
		if (!handle) return;
		$.RegisterEventHandler("DragStart", handle, _BeginDrag);
		$.RegisterEventHandler("DragEnd", handle, _EndDrag);
		_dragEventsRegistered = true;
	}

	function ResetPosition() {
		var dock = _Panel("SwiftDemoVoiceDock");
		var menu = _Panel("SwiftDemoVoiceMenu");
		_customPosition = false;
		_customPositionX = 0;
		_customPositionY = 0;
		_customPositionRatioX = 0;
		_customPositionRatioY = 0;
		if (dock) {
			dock.style.position = "0px 0px 0px";
			_SetClass(dock, "custom-position", false);
			_SetClass(dock, "dragging", false);
		}
		_SetClass(menu, "custom-position", false);
		$.Msg("[SwiftDemoVoice] panel position reset");
	}

	function _UpdateViewportClass() {
		var metrics = _ViewportMetrics();
		var menu = _Panel("SwiftDemoVoiceMenu");
		var dock = _Panel("SwiftDemoVoiceDock");
		if (!metrics || !menu) return;
		var profile = _ViewportProfile(metrics.width, metrics.height);
		if (!profile) return;
		var sizeChanged = Math.abs(metrics.width - _lastViewportWidth) > 1
			|| Math.abs(metrics.height - _lastViewportHeight) > 1;
		if (profile === _lastViewportProfile && !sizeChanged) return;
		_lastViewportProfile = profile;
		_lastViewportWidth = metrics.width;
		_lastViewportHeight = metrics.height;
		_SetClass(menu, "swift-aspect-4x3", profile === "4x3");
		_SetClass(dock, "swift-aspect-4x3", profile === "4x3");
		if (_customPosition && sizeChanged) $.Schedule(0.0, _ReflowCustomPosition);
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

	function _FilterSpeakingSlotsForSelection(slots) {
		var result = [];
		if (!slots || _selectionMode === "none") return result;
		for (var index = 0; index < slots.length; index++) {
			var slot = _NormalizeSlot(slots[index]);
			if (slot < 0) continue;
			if (_selectionMode === "all" || _selectedSlots[slot]) result.push(slot);
		}
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
			slots = data && state
				? _FilterSpeakingSlotsForSelection(_SpeakingSlotsForTick(state.nTick, data))
				: [];
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

	function _SetRowPovState(row, player, observed) {
		var povLabel = row ? row.FindChildTraverse("SwiftDemoVoicePovLabel") : null;
		if (!povLabel || !player) return;
		povLabel.text = _Localize(observed && player.canFocus
			? "#SwiftDemoVoice_Current"
			: (player.canFocus
				? "#SwiftDemoVoice_View"
				: (player.isDead ? "#SwiftDemoVoice_Dead" : "#SwiftDemoVoice_NotAvailable")));
	}

	function _RenderObservedState() {
		for (var i = 0; i < _players.length; i++) {
			var player = _players[i];
			var row = _Panel("SwiftDemoVoicePlayer_" + player.slot);
			if (!row) continue;
			var observed = !!_lastHudPlayerXuid && player.xuid === _lastHudPlayerXuid;
			_SetClass(row, "observed", observed);
			_SetRowPovState(row, player, observed);
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
			var playerAvatar = row.FindChildTraverse("SwiftDemoVoicePlayerAvatar");
			if (player.xuid && playerAvatar && playerAvatar.PopulateFromSteamID) {
				try {
					playerAvatar.PopulateFromSteamID(player.xuid);
				} catch (error) {
					$.Msg("[SwiftDemoVoice] unable to populate player avatar: " + error);
				}
			}
			var playerStatus = player.isDead
				? _Localize("#SwiftDemoVoice_Dead")
				: (player.isDisconnected ? _Localize("#SwiftDemoVoice_Offline") : "");
			var meta = _Localize(
				playerStatus ? "#SwiftDemoVoice_PlayerMetaStatus" : "#SwiftDemoVoice_PlayerMeta",
				{ team: _TeamLabel(player.team), slot: player.slot + 1, status: playerStatus }
			);
			row.FindChildTraverse("SwiftDemoVoicePlayerMeta").text = meta;

			var povIcon = row.FindChildTraverse("SwiftDemoVoicePovIcon");
			if (povIcon) povIcon.SetImage(player.canFocus
				? "s2r://panorama/images/icons/ui/watch.vsvg"
				: "s2r://panorama/images/icons/ui/elimination.vsvg");
			_SetRowPovState(row, player, false);

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
						TogglePlayer(playerSlot, playerName, playerXuid);
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

	function _SetPlayerAvatarMode(showAvatars) {
		_showPlayerAvatars = !!showAvatars;
		var menu = _Panel("SwiftDemoVoiceMenu");
		var control = _Panel("SwiftDemoVoiceAvatarMode");
		var toggle = _Panel("SwiftDemoVoiceAvatarToggle");
		_SetClass(menu, "avatar-mode", _showPlayerAvatars);
		_SetClass(control, "selected", _showPlayerAvatars);
		if (toggle) toggle.selected = _showPlayerAvatars;
		return _showPlayerAvatars;
	}

	function TogglePlayerAvatarMode() {
		return _SetPlayerAvatarMode(!_showPlayerAvatars);
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

	function _UnmuteNativePlayer(player) {
		if (!player || !player.xuid || !GameStateAPI.IsSelectedPlayerMuted || !GameStateAPI.ToggleMute) return false;
		try {
			if (!GameStateAPI.IsSelectedPlayerMuted(player.xuid)) return false;
			GameStateAPI.ToggleMute(player.xuid);
			var unmuted = !GameStateAPI.IsSelectedPlayerMuted(player.xuid);
			$.Msg("[SwiftDemoVoice] native mute " + (unmuted ? "cleared" : "retained") +
				" slot=" + (player.slot + 1) + " xuid=" + player.xuid);
			return unmuted;
		} catch (error) {
			$.Msg("[SwiftDemoVoice] native unmute failed for " + player.xuid + ": " + error);
			return false;
		}
	}

	function _UnmuteSelectedPlayers() {
		for (var i = 0; i < _players.length; i++) {
			var player = _players[i];
			if (!_selectedSlots[player.slot]) continue;
			_nativeUnmuteSeenPlayers[player.xuid] = true;
			_UnmuteNativePlayer(player);
		}
	}

	function _UnmuteNewDemoPlayers(isDemo) {
		if (!isDemo) {
			_nativeUnmuteSeenPlayers = {};
			return;
		}
		for (var i = 0; i < _players.length; i++) {
			var player = _players[i];
			if (!player.xuid || _nativeUnmuteSeenPlayers[player.xuid]) continue;
			_nativeUnmuteSeenPlayers[player.xuid] = true;
			_UnmuteNativePlayer(player);
		}
	}

	function _ApplyCustomSelection(status) {
		var masks = BuildMasksForSlots(_SelectedSlotArray());
		_RunMaskCommands(masks.low, masks.high, status);
		_RenderSelection();
	}

	function SelectAll() {
		_selectionMode = "all";
		_PopulateAllVisibleSelections();
		_UnmuteSelectedPlayers();
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
		_UnmuteSelectedPlayers();
		_ApplyCustomSelection(_Localize(team === "CT"
			? "#SwiftDemoVoice_ListeningCounterTerrorists"
			: "#SwiftDemoVoice_ListeningTerrorists"));
	}

	function TogglePlayer(slot, playerName, playerXuid) {
		if (_selectionMode === "all") _PopulateAllVisibleSelections();
		else if (_selectionMode === "none") _selectedSlots = {};
		_selectionMode = "custom";
		_selectedSlots[slot] = !_selectedSlots[slot];
		if (_selectedSlots[slot]) {
			for (var i = 0; i < _players.length; i++) {
				if (_players[i].slot === slot || (playerXuid && _players[i].xuid === String(playerXuid))) {
					_nativeUnmuteSeenPlayers[_players[i].xuid] = true;
					_UnmuteNativePlayer(_players[i]);
					break;
				}
			}
		}
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
		var dock = _Panel("SwiftDemoVoiceDock");
		_SetClass(menu, "collapsed", !_isOpen);
		_SetClass(dock, "collapsed", !_isOpen);
		if (!_isOpen) _SetRoundPickerOpen(false);
		if (_customPosition) $.Schedule(0.0, _ClampCustomPosition);
	}

	function ToggleOpen() {
		_SetOpen(!_isOpen);
	}

	function _MountNativeMenuToggle() {
		if (_nativeMenuToggleMounted) return true;
		var toggle = _Panel("SwiftDemoVoiceMenuToggle");
		var controlRow = _Panel("ControlRow");
		var speedControls = _Panel("SpeedControls");
		if (!toggle || !controlRow || !speedControls || !toggle.SetParent || !controlRow.MoveChildAfter) return false;
		toggle.SetParent(controlRow);
		controlRow.MoveChildAfter(toggle, speedControls);
		_SetClass(toggle, "native-mounted", true);
		_nativeMenuToggleMounted = true;
		return true;
	}

	function _SetMenuVisible(visible) {
		_isMenuVisible = !!visible;
		var dock = _Panel("SwiftDemoVoiceDock");
		var toggle = _Panel("SwiftDemoVoiceMenuToggle");
		_SetClass(dock, "menu-hidden", !_isMenuVisible);
		_SetClass(toggle, "selected", _isMenuVisible);
		if (_isMenuVisible && _customPosition) $.Schedule(0.0, _ClampCustomPosition);
	}

	function ToggleMenuVisible() {
		if (!_wasDemo && !_IsDemoPlayback()) return false;
		_SetMenuVisible(!_isMenuVisible);
		$.Msg("[SwiftDemoVoice] menu " + (_isMenuVisible ? "shown" : "hidden") + " by native DemoUI control");
		return true;
	}

	function _Poll() {
		var isDemo = _IsDemoPlayback();
		var menu = _Panel("SwiftDemoVoiceMenu");
		if (!_nativeMenuToggleMounted) _MountNativeMenuToggle();
		_UpdateViewportClass();
		// CSGOHudDemoController itself is hidden by the engine outside demo playback.
		// Keep this child visible and refresh data even if one demo-state API lags.
		_SetClass(menu, "demo-active", true);
		_lastHudPlayerXuid = isDemo ? _GetHudPlayerXuid() : "";
		Refresh(false);
		_UpdateRoundPicker(false);

		if (isDemo && !_wasDemo) {
			_wasDemo = true;
			_nativeUnmuteSeenPlayers = {};
			_SetMenuVisible(true);
			_SetOpen(true);
			Refresh(true);
			SelectAll();
			$.Msg("[SwiftDemoVoice] demo detected; all voice slots enabled");
		} else if (!isDemo && _wasDemo) {
			_wasDemo = false;
			_nativeUnmuteSeenPlayers = {};
			_lastPlayerSignature = "";
		}
		_UnmuteNewDemoPlayers(isDemo);

		$.Schedule(0.75, _Poll);
	}

	function OnLoad() {
		if (_started) return;
		_started = true;
		_UpdateViewportClass();
		_SetClass(_Panel("SwiftDemoVoiceMenu"), "demo-active", true);
		_SetupDragging();
		_MountNativeMenuToggle();
		_SetMenuVisible(true);
		_SetOpen(true);
		_SetPlayerAvatarMode(false);
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
		TogglePlayerAvatarMode: TogglePlayerAvatarMode,
		FocusPlayer: FocusPlayer,
		RoundNumberForTick: _RoundNumberForTick,
		SpeakingSlotsForTick: _SpeakingSlotsForTick,
		FilterSpeakingSlotsForSelection: _FilterSpeakingSlotsForSelection,
		ViewportProfileForTest: _ViewportProfile,
		ResolvePositionForTest: _ResolvePosition,
		JumpToRound: JumpToRound,
		ToggleRoundPicker: ToggleRoundPicker,
		Refresh: Refresh,
		ToggleOpen: ToggleOpen,
		ToggleMenuVisible: ToggleMenuVisible,
		ResetPosition: ResetPosition,
		OnLoad: OnLoad
	};
})();

// Some CS2 builds do not dispatch XML Panel onload consistently for panels that
// start collapsed. Boot from the included script as well; OnLoad is idempotent.
$.Schedule(0.0, SwiftDemoVoice.OnLoad);
