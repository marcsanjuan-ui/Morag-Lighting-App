import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart' hide colorToHex;
import 'package:image_picker/image_picker.dart';

import '../light_controller.dart';
import '../models.dart';
import '../storage.dart';
import 'home_screen.dart';

Future<void> openBrandEditor(BuildContext context, LightController ctrl, Preset? preset) {
  return Navigator.of(context).push(
    MaterialPageRoute(builder: (_) => BrandEditor(ctrl: ctrl, original: preset)),
  );
}

/// Add brand / edit brand: logo, name, speed, brightness, colors, effect.
class BrandEditor extends StatefulWidget {
  const BrandEditor({super.key, required this.ctrl, this.original});
  final LightController ctrl;
  final Preset? original;

  @override
  State<BrandEditor> createState() => _BrandEditorState();
}

class _BrandEditorState extends State<BrandEditor> {
  late Preset p;
  late final TextEditingController _name;
  int _slot = 0; // which brand color the wheel is editing
  String? _pickedLogo; // new logo picked in this session, saved on Save

  bool get isNew => widget.original == null;

  @override
  void initState() {
    super.initState();
    p = widget.original?.copy() ??
        Preset(
          id: DateTime.now().microsecondsSinceEpoch.toString(),
          name: '',
          colors: [const Color(0xFF6C3BD1)],
        );
    _name = TextEditingController(text: p.name);
  }

  @override
  void dispose() {
    _name.dispose();
    super.dispose();
  }

  String? get _logoShown => _pickedLogo ?? p.logoPath;

  Future<void> _pickLogo() async {
    try {
      final file = await ImagePicker().pickImage(source: ImageSource.gallery, maxWidth: 512, maxHeight: 512);
      if (file != null) setState(() => _pickedLogo = file.path);
    } catch (_) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Could not open the gallery.')));
      }
    }
  }

  Future<void> _save() async {
    final name = _name.text.trim();
    if (name.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Give the brand a name.')));
      return;
    }
    p.name = name;
    if (_pickedLogo != null) {
      final old = p.logoPath;
      p.logoPath = await AppStorage.saveLogo(p.id, _pickedLogo!);
      await AppStorage.deleteLogo(old);
    }
    await widget.ctrl.savePreset(p);
    if (mounted) Navigator.pop(context);
  }

  Future<void> _delete() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text('Delete ${p.name}?'),
        content: const Text('This removes the brand from this phone.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Delete')),
        ],
      ),
    );
    if (ok == true) {
      await AppStorage.deleteLogo(p.logoPath);
      await widget.ctrl.deletePreset(p.id);
      if (mounted) Navigator.pop(context);
    }
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final logo = _logoShown;
    return Scaffold(
      appBar: AppBar(
        title: Text(isNew ? 'Add brand' : 'Edit brand'),
        actions: [
          if (!isNew) IconButton(onPressed: _delete, icon: const Icon(Icons.delete_outline), tooltip: 'Delete'),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 8, 16, 32),
        children: [
          // Logo
          Center(
            child: InkWell(
              customBorder: const CircleBorder(),
              onTap: _pickLogo,
              child: CircleAvatar(
                radius: 64,
                backgroundColor: scheme.surfaceContainerHighest,
                backgroundImage: logo != null && File(logo).existsSync() ? FileImage(File(logo)) : null,
                child: logo == null
                    ? Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Icon(Icons.add_photo_alternate_outlined, color: scheme.onSurfaceVariant),
                          Text('logo', style: TextStyle(color: scheme.onSurfaceVariant)),
                        ],
                      )
                    : null,
              ),
            ),
          ),
          if (logo != null)
            Center(
              child: TextButton(
                onPressed: () => setState(() {
                  _pickedLogo = null;
                  p.logoPath = null;
                }),
                child: const Text('Remove logo'),
              ),
            ),
          const SizedBox(height: 12),

          // Name
          TextField(
            controller: _name,
            textAlign: TextAlign.center,
            textCapitalization: TextCapitalization.words,
            decoration: const InputDecoration(labelText: 'Name', border: OutlineInputBorder()),
          ),
          const SizedBox(height: 12),

          LabeledSlider(label: 'Speed', value: p.speed, min: 1, onChanged: (v) => setState(() => p.speed = v)),
          LabeledSlider(
            label: 'Brightness',
            value: p.brightness,
            min: 0,
            suffix: '%',
            onChanged: (v) => setState(() => p.brightness = v),
          ),
          const SizedBox(height: 8),

          // Colors: pick a slot, then set it with the wheel
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Text('Color'),
              const SizedBox(width: 12),
              for (var i = 0; i < p.colors.length; i++)
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 4),
                  child: GestureDetector(
                    onTap: () => setState(() => _slot = i),
                    onLongPress: i == 0
                        ? null
                        : () => setState(() {
                              p.colors.removeAt(i);
                              _slot = 0;
                            }),
                    child: Container(
                      width: 36,
                      height: 36,
                      decoration: BoxDecoration(
                        color: p.colors[i],
                        shape: BoxShape.circle,
                        border: Border.all(
                          color: _slot == i ? scheme.primary : scheme.outlineVariant,
                          width: _slot == i ? 3 : 1,
                        ),
                      ),
                    ),
                  ),
                ),
              if (p.colors.length < 3)
                IconButton(
                  tooltip: 'Add a color',
                  onPressed: () => setState(() {
                    p.colors.add(Colors.white);
                    _slot = p.colors.length - 1;
                  }),
                  icon: const Icon(Icons.add_circle_outline),
                ),
            ],
          ),
          Text(
            '${const ['Primary', 'Secondary', 'Accent'][_slot]} color ${colorToHex(p.colors[_slot])}'
            '${p.colors.length > 1 ? ' · long-press a dot to remove it' : ''}',
            textAlign: TextAlign.center,
            style: TextStyle(color: scheme.onSurfaceVariant, fontSize: 12),
          ),
          Center(
            child: ColorPicker(
              key: ValueKey(_slot),
              pickerColor: p.colors[_slot],
              onColorChanged: (c) => setState(() => p.colors[_slot] = c),
              paletteType: PaletteType.hueWheel,
              enableAlpha: false,
              hexInputBar: true,
              labelTypes: const [],
              colorPickerWidth: 240,
              pickerAreaHeightPercent: 1.0,
            ),
          ),
          const SizedBox(height: 8),

          EffectDropdown(value: p.effect, onChanged: (e) => setState(() => p.effect = e)),
          const SizedBox(height: 8),
          Text(
            'LEDs show color differently from screens. Preview on the strip and adjust by eye.',
            textAlign: TextAlign.center,
            style: TextStyle(color: scheme.onSurfaceVariant, fontSize: 12),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () => widget.ctrl.previewPreset(p),
                  icon: const Icon(Icons.visibility_outlined),
                  label: const Text('Preview'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: FilledButton.icon(
                  onPressed: _save,
                  icon: const Icon(Icons.check),
                  label: const Text('Save'),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}