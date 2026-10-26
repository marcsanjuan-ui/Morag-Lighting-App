import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart' hide colorToHex;

import '../light_controller.dart';
import '../main.dart';
import '../models.dart';
import 'brand_editor.dart';
import 'connect_screen.dart';

/// Home: color wheel, speed, brightness, effects, and the brand grid.
class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late final LightController ctrl = LightController(controllerApi);

  @override
  void initState() {
    super.initState();
    ctrl.init();
  }

  @override
  void dispose() {
    ctrl.dispose();
    super.dispose();
  }

  void _openProfile() => Navigator.of(context).push(
        MaterialPageRoute(builder: (_) => const ConnectScreen(profile: true)),
      );

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: ctrl,
      builder: (context, _) {
        final s = ctrl.state;
        final text = Theme.of(context).textTheme;
        final scheme = Theme.of(context).colorScheme;
        return Scaffold(
          appBar: AppBar(
            leading: Padding(
              padding: const EdgeInsets.all(8),
              child: InkWell(
                customBorder: const CircleBorder(),
                onTap: _openProfile,
                child: CircleAvatar(
                  backgroundColor: scheme.primaryContainer,
                  child: Icon(Icons.person, color: scheme.onPrimaryContainer, size: 20),
                ),
              ),
            ),
            title: const Text('Home'),
            centerTitle: true,
            actions: [
              Switch(value: s.power, onChanged: ctrl.setPower),
              const SizedBox(width: 8),
            ],
          ),
          body: RefreshIndicator(
            onRefresh: ctrl.refreshStatus,
            child: ListView(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 32),
              children: [
                if (ctrl.lastError != null)
                  Card(
                    color: scheme.errorContainer,
                    child: ListTile(
                      dense: true,
                      leading: Icon(Icons.error_outline, color: scheme.onErrorContainer),
                      title: Text(ctrl.lastError!, style: TextStyle(color: scheme.onErrorContainer)),
                      trailing: TextButton(onPressed: ctrl.refreshStatus, child: const Text('Retry')),
                    ),
                  ),
                Text('Now showing: ${s.name}',
                    textAlign: TextAlign.center, style: TextStyle(color: scheme.onSurfaceVariant)),
                const SizedBox(height: 8),

                // Color wheel
                Center(
                  child: ColorPicker(
                    pickerColor: s.colors[0],
                    onColorChanged: ctrl.setColor,
                    paletteType: PaletteType.hueWheel,
                    enableAlpha: false,
                    labelTypes: const [],
                    colorPickerWidth: 260,
                    pickerAreaHeightPercent: 1.0,
                  ),
                ),

                // Speed, brightness, effects
                LabeledSlider(label: 'Speed', value: s.speed, min: 1, onChanged: ctrl.setSpeed),
                LabeledSlider(label: 'Brightness', value: s.brightness, min: 0, suffix: '%', onChanged: ctrl.setBrightness),
                const SizedBox(height: 8),
                EffectDropdown(value: s.effect, onChanged: ctrl.setEffect),
                const SizedBox(height: 24),

                // Brands
                Text('Brands', style: text.titleMedium, textAlign: TextAlign.center),
                Text('Tap to apply · long-press to edit',
                    textAlign: TextAlign.center, style: TextStyle(color: scheme.onSurfaceVariant, fontSize: 12)),
                const SizedBox(height: 12),
                GridView.count(
                  crossAxisCount: 3,
                  shrinkWrap: true,
                  physics: const NeverScrollableScrollPhysics(),
                  mainAxisSpacing: 12,
                  crossAxisSpacing: 12,
                  childAspectRatio: 0.82,
                  children: [
                    for (final p in ctrl.presets)
                      BrandTile(
                        preset: p,
                        active: ctrl.activePresetId == p.id,
                        onTap: () => ctrl.applyPreset(p),
                        onLongPress: () => openBrandEditor(context, ctrl, p),
                      ),
                    _AddBrandTile(onTap: () => openBrandEditor(context, ctrl, null)),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

/// A slider with its label above, as on the wireframe.
class LabeledSlider extends StatelessWidget {
  const LabeledSlider({
    super.key,
    required this.label,
    required this.value,
    required this.min,
    required this.onChanged,
    this.suffix = '',
  });
  final String label;
  final int value;
  final int min;
  final String suffix;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8),
          child: Row(
            children: [
              Expanded(child: Text(label, textAlign: TextAlign.center)),
              Text('$value$suffix', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant)),
            ],
          ),
        ),
        Slider(
          value: value.clamp(min, 100).toDouble(),
          min: min.toDouble(),
          max: 100,
          divisions: 100 - min,
          label: '$value$suffix',
          onChanged: (v) => onChanged(v.round()),
        ),
      ],
    );
  }
}

/// "effects: [ dropdown ]"
class EffectDropdown extends StatelessWidget {
  const EffectDropdown({super.key, required this.value, required this.onChanged});
  final String value;
  final ValueChanged<String> onChanged;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        const SizedBox(width: 8),
        const Text('Effects:'),
        const SizedBox(width: 12),
        Expanded(
          child: DropdownButtonFormField<String>(
            // ignore: deprecated_member_use
            value: value,
            isDense: true,
            decoration: const InputDecoration(border: OutlineInputBorder(), isDense: true),
            items: [
              for (final e in kEffects) DropdownMenuItem(value: e, child: Text(kEffectLabels[e]!)),
            ],
            onChanged: (v) {
              if (v != null) onChanged(v);
            },
          ),
        ),
      ],
    );
  }
}

class BrandTile extends StatelessWidget {
  const BrandTile({super.key, required this.preset, required this.active, required this.onTap, required this.onLongPress});
  final Preset preset;
  final bool active;
  final VoidCallback onTap;
  final VoidCallback onLongPress;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final cols = preset.colors.length == 1 ? [preset.colors[0], preset.colors[0]] : preset.colors;
    final logo = preset.logoPath;
    return Column(
      children: [
        Expanded(
          child: Material(
            clipBehavior: Clip.antiAlias,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(16),
              side: BorderSide(color: active ? scheme.primary : scheme.outlineVariant, width: active ? 3 : 1),
            ),
            child: InkWell(
              onTap: onTap,
              onLongPress: onLongPress,
              child: Stack(
                fit: StackFit.expand,
                children: [
                  DecoratedBox(decoration: BoxDecoration(gradient: LinearGradient(colors: cols))),
                  if (logo != null && File(logo).existsSync())
                    Padding(
                      padding: const EdgeInsets.all(10),
                      child: Image.file(File(logo), fit: BoxFit.contain),
                    ),
                ],
              ),
            ),
          ),
        ),
        const SizedBox(height: 4),
        Text(preset.name, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(fontSize: 12)),
      ],
    );
  }
}

class _AddBrandTile extends StatelessWidget {
  const _AddBrandTile({required this.onTap});
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Column(
      children: [
        Expanded(
          child: Material(
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(16),
              side: BorderSide(color: scheme.outline),
            ),
            child: InkWell(
              customBorder: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
              onTap: onTap,
              child: Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.add, color: scheme.primary),
                    const Text('add brand', textAlign: TextAlign.center),
                  ],
                ),
              ),
            ),
          ),
        ),
        const SizedBox(height: 4),
        const Text('', style: TextStyle(fontSize: 12)),
      ],
    );
  }
}