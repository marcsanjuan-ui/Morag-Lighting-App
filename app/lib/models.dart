import 'package:flutter/material.dart';

/// Effects the controller understands (same names as the firmware).
const List<String> kEffects = ['solid', 'breathe', 'wipe', 'chase', 'gradient', 'rainbow'];

const Map<String, String> kEffectLabels = {
  'solid': 'Solid',
  'breathe': 'Breathing',
  'wipe': 'Color wipe',
  'chase': 'Chase',
  'gradient': 'Gradient',
  'rainbow': 'Rainbow',
};

String colorToHex(Color c) {
  // ignore: deprecated_member_use
  final v = c.value & 0xFFFFFF;
  return '#${v.toRadixString(16).padLeft(6, '0').toUpperCase()}';
}

Color? hexToColor(String hex) {
  var h = hex.trim().replaceFirst('#', '');
  if (h.length != 6) return null;
  final v = int.tryParse(h, radix: 16);
  if (v == null) return null;
  return Color(0xFF000000 | v);
}

/// One brand preset: up to three colors plus an effect.
class Preset {
  Preset({
    required this.id,
    required this.name,
    required this.colors,
    this.effect = 'solid',
    this.speed = 50,
    this.brightness = 70,
    this.logoPath,
  });

  final String id;
  String name;
  List<Color> colors; // 1 to 3 colors
  String effect;
  int speed; // 1-100
  int brightness; // 0-100
  String? logoPath; // brand logo saved on the phone

  /// The single command that applies this preset on the controller.
  String toCommand() {
    final safeName = name.replaceAll(',', ' ').trim();
    final cols = colors.take(3).map(colorToHex).join(',');
    return 'PRESET,$safeName,$effect,$speed,$brightness,$cols';
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'colors': colors.map(colorToHex).toList(),
        'effect': effect,
        'speed': speed,
        'brightness': brightness,
        'logo': logoPath,
      };

  factory Preset.fromJson(Map<String, dynamic> j) => Preset(
        id: j['id'] as String,
        name: j['name'] as String,
        colors: (j['colors'] as List)
            .map((e) => hexToColor(e as String) ?? Colors.white)
            .toList(),
        effect: kEffects.contains(j['effect']) ? j['effect'] as String : 'solid',
        speed: (j['speed'] as num?)?.toInt() ?? 50,
        brightness: (j['brightness'] as num?)?.toInt() ?? 70,
        logoPath: j['logo'] as String?,
      );

  Preset copy() => Preset(
        id: id,
        name: name,
        colors: List.of(colors),
        effect: effect,
        speed: speed,
        brightness: brightness,
        logoPath: logoPath,
      );
}

/// What the strip is doing right now, as reported by STATE replies.
class LightState {
  bool power = true;
  int brightness = 60;
  String effect = 'solid';
  int speed = 50;
  List<Color> colors = [Colors.white, Colors.black, Colors.black];
  String name = 'Custom';

  /// Parses "STATE,<power>,<bright>,<effect>,<speed>,#c1,#c2,#c3,<name>".
  bool applyStateLine(String line) {
    final p = line.trim().split(',');
    if (p.length < 8 || p[0] != 'STATE') return false;
    power = p[1] == '1';
    brightness = int.tryParse(p[2]) ?? brightness;
    effect = kEffects.contains(p[3]) ? p[3] : effect;
    speed = int.tryParse(p[4]) ?? speed;
    colors = [
      hexToColor(p[5]) ?? Colors.white,
      hexToColor(p[6]) ?? Colors.black,
      hexToColor(p[7]) ?? Colors.black,
    ];
    name = p.length > 8 ? p.sublist(8).join(',') : 'Custom';
    return true;
  }
}
