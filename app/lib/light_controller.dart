import 'package:flutter/material.dart';

import 'api.dart';
import 'models.dart';
import 'storage.dart';

/// App state: what the strip is doing, the brand presets, and the link.
class LightController extends ChangeNotifier {
  LightController(this.api) {
    queue = CommandQueue(api, onReply: _onReply, onError: _onError);
  }

  final ControllerApi api;
  late final CommandQueue queue;

  final LightState state = LightState();
  List<Preset> presets = [];
  String? activePresetId;
  String? lastError;
  bool online = true;

  Future<void> init() async {
    presets = await AppStorage.loadPresets();
    notifyListeners();
    await refreshStatus();
  }

  Future<void> refreshStatus() async {
    try {
      final line = await api.status();
      state.applyStateLine(line);
      online = true;
      lastError = null;
    } on ApiException catch (e) {
      _onError(e);
    }
    notifyListeners();
  }

  void _onReply(String reply) {
    if (!online || lastError != null) {
      online = true;
      lastError = null;
      notifyListeners();
    }
  }

  void _onError(ApiException e) {
    lastError = e.message;
    online = e.statusCode != null; // a reply came back, so the link works
    notifyListeners();
  }

  // ---- Live controls ----
  void setPower(bool on) {
    state.power = on;
    queue.send('power', 'POWER,${on ? 1 : 0}');
    notifyListeners();
  }

  void setColor(Color c) {
    state.colors[0] = c;
    state.effect = 'solid';
    state.power = true;
    state.name = 'Custom';
    activePresetId = null;
    queue.send('color', 'COLOR,${colorToHex(c)}');
    notifyListeners();
  }

  void setBrightness(int pct) {
    state.brightness = pct;
    queue.send('bright', 'BRIGHT,$pct');
    notifyListeners();
  }

  void setEffect(String effect) {
    state.effect = effect;
    activePresetId = null;
    queue.send('effect', 'EFFECT,$effect,${state.speed}');
    notifyListeners();
  }

  void setSpeed(int speed) {
    state.speed = speed;
    queue.send('effect', 'EFFECT,${state.effect},$speed');
    notifyListeners();
  }

  void applyPreset(Preset p) {
    activePresetId = p.id;
    state
      ..power = true
      ..name = p.name
      ..effect = p.effect
      ..speed = p.speed
      ..brightness = p.brightness
      ..colors = [
        p.colors.isNotEmpty ? p.colors[0] : Colors.white,
        p.colors.length > 1 ? p.colors[1] : Colors.black,
        p.colors.length > 2 ? p.colors[2] : Colors.black,
      ];
    queue.send('preset', p.toCommand());
    notifyListeners();
  }

  /// Sends a preset without marking it active (used by the editor's preview).
  void previewPreset(Preset p) => queue.send('preset', p.toCommand());

  // ---- Preset list ----
  Future<void> savePreset(Preset p) async {
    final i = presets.indexWhere((e) => e.id == p.id);
    if (i >= 0) {
      presets[i] = p;
    } else {
      presets.add(p);
    }
    await AppStorage.savePresets(presets);
    notifyListeners();
  }

  Future<void> deletePreset(String id) async {
    presets.removeWhere((e) => e.id == id);
    if (activePresetId == id) activePresetId = null;
    await AppStorage.savePresets(presets);
    notifyListeners();
  }

  Future<void> movePreset(int oldIndex, int newIndex) async {
    if (newIndex > oldIndex) newIndex -= 1;
    final p = presets.removeAt(oldIndex);
    presets.insert(newIndex, p);
    await AppStorage.savePresets(presets);
    notifyListeners();
  }
}
