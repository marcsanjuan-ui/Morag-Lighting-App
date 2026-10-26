import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:morag_lighting/models.dart';

void main() {
  test('preset builds the controller command', () {
    final p = Preset(
      id: 'x',
      name: 'Brand, B',
      colors: const [Color(0xFF0057B8), Color(0xFFFFC72C)],
      effect: 'gradient',
      speed: 50,
      brightness: 80,
    );
    expect(p.toCommand(), 'PRESET,Brand  B,gradient,50,80,#0057B8,#FFC72C');
  });

  test('STATE reply updates the light state', () {
    final s = LightState();
    expect(s.applyStateLine('STATE,0,80,chase,30,#E4002B,#FFFFFF,#000000,Brand A'), isTrue);
    expect(s.power, isFalse);
    expect(s.brightness, 80);
    expect(s.effect, 'chase');
    expect(colorToHex(s.colors[0]), '#E4002B');
    expect(s.name, 'Brand A');
  });
}