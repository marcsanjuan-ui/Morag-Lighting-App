import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'models.dart';

/// Brand presets and app settings, stored on the phone.
class AppStorage {
  static const _kPresets = 'presets_v1';
  static const _kUser = 'last_user';
  static const _kSsid = 'wifi_ssid';
  static const _kWifiPass = 'wifi_pass';
  static const _kHost = 'device_host';
  static const _kSiteSsid = 'site_ssid';

  static const defaultSsid = 'MORAG-Light';
  static const defaultWifiPass = 'morag2026';

  /// Example presets from the proposal. Replace with MORAG's real brand colors.
  static List<Preset> examplePresets() => [
        Preset(
          id: 'brand-a',
          name: 'Brand A',
          colors: const [Color(0xFFE4002B), Color(0xFFFFFFFF)],
          effect: 'breathe',
          speed: 25,
          brightness: 70,
        ),
        Preset(
          id: 'brand-b',
          name: 'Brand B',
          colors: const [Color(0xFF0057B8), Color(0xFFFFC72C)],
          effect: 'gradient',
          speed: 50,
          brightness: 80,
        ),
        Preset(
          id: 'brand-c',
          name: 'Brand C',
          colors: const [Color(0xFF00A651), Color(0xFF000000), Color(0xFFFFFFFF)],
          effect: 'chase',
          speed: 80,
          brightness: 60,
        ),
      ];

  static Future<List<Preset>> loadPresets() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_kPresets);
    if (raw == null) return examplePresets();
    try {
      return (jsonDecode(raw) as List)
          .map((e) => Preset.fromJson(e as Map<String, dynamic>))
          .toList();
    } catch (_) {
      return examplePresets();
    }
  }

  static Future<void> savePresets(List<Preset> presets) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_kPresets, jsonEncode(presets.map((p) => p.toJson()).toList()));
  }

  static Future<String> lastUser() async =>
      (await SharedPreferences.getInstance()).getString(_kUser) ?? '';

  static Future<void> setLastUser(String user) async =>
      (await SharedPreferences.getInstance()).setString(_kUser, user);

  static Future<(String, String)> wifi() async {
    final prefs = await SharedPreferences.getInstance();
    return (prefs.getString(_kSsid) ?? defaultSsid, prefs.getString(_kWifiPass) ?? defaultWifiPass);
  }

  /// The address the app last signed in to (device IP on the site Wi-Fi, or 192.168.4.1).
  static Future<String?> host() async => (await SharedPreferences.getInstance()).getString(_kHost);

  static Future<void> setHost(String host) async =>
      (await SharedPreferences.getInstance()).setString(_kHost, host);

  /// Last site Wi-Fi name typed in (the password is not kept).
  static Future<String> siteSsid() async =>
      (await SharedPreferences.getInstance()).getString(_kSiteSsid) ?? '';

  static Future<void> setSiteSsid(String ssid) async =>
      (await SharedPreferences.getInstance()).setString(_kSiteSsid, ssid);

  /// Copies a picked logo into the app's own folder and returns its path.
  static Future<String> saveLogo(String presetId, String sourcePath) async {
    final dir = Directory('${(await getApplicationDocumentsDirectory()).path}/logos');
    await dir.create(recursive: true);
    final ext = sourcePath.contains('.') ? sourcePath.substring(sourcePath.lastIndexOf('.')) : '.png';
    final target = '${dir.path}/$presetId-${DateTime.now().millisecondsSinceEpoch}$ext';
    await File(sourcePath).copy(target);
    return target;
  }

  static Future<void> deleteLogo(String? path) async {
    if (path == null) return;
    try {
      await File(path).delete();
    } catch (_) {}
  }

  static Future<void> setWifi(String ssid, String pass) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_kSsid, ssid);
    await prefs.setString(_kWifiPass, pass);
  }
}