import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

/// Thrown when the controller replies with an error.
class ApiException implements Exception {
  ApiException(this.message, {this.statusCode});
  final String message;
  final int? statusCode;
  bool get isUnauthorized => statusCode == 401;
  @override
  String toString() => message;
}

/// What /api/info reports about the controller's Wi-Fi.
class DeviceInfo {
  DeviceInfo({
    required this.apSsid,
    required this.apIp,
    required this.viaAp,
    required this.staSsid,
    required this.staStatus,
    required this.staIp,
  });

  final String apSsid; // the device AP name
  final String apIp; // 192.168.4.1
  final bool viaAp; // this phone is talking over the device AP
  final String staSsid; // site Wi-Fi the device is set to join
  final String staStatus; // none | connecting | connected | failed | not_found
  final String staIp; // device IP on the site Wi-Fi

  bool get siteConnected => staStatus == 'connected' && staIp.isNotEmpty;

  factory DeviceInfo.fromJson(Map<String, dynamic> j) => DeviceInfo(
        apSsid: (j['ap_ssid'] ?? '') as String,
        apIp: (j['ap_ip'] ?? '192.168.4.1') as String,
        viaAp: (j['via_ap'] ?? false) as bool,
        staSsid: (j['sta_ssid'] ?? '') as String,
        staStatus: (j['sta_status'] ?? 'none') as String,
        staIp: (j['sta_ip'] ?? '') as String,
      );
}

/// Talks to the ESP32 controller, on its own AP or on the site Wi-Fi.
class ControllerApi {
  ControllerApi({String host = apHost}) : _host = host;

  static const apHost = '192.168.4.1';

  String _host;
  String get host => _host;
  set host(String h) => _host = h.trim().replaceFirst(RegExp(r'^https?://'), '').replaceAll('/', '');
  String get baseUrl => 'http://$_host';
  String? _token;
  final http.Client _client = http.Client();

  /// Called when the controller says the login has expired.
  void Function()? onUnauthorized;

  bool get signedIn => _token != null;

  Future<String> _post(String path, Map<String, String> body, {Duration timeout = const Duration(seconds: 5)}) async {
    final http.Response res;
    try {
      res = await _client
          .post(Uri.parse('$baseUrl$path'), body: body)
          .timeout(timeout);
    } on TimeoutException {
      throw ApiException('The controller did not answer. Check that the phone is on the controller Wi-Fi.');
    } catch (_) {
      throw ApiException('Cannot reach the controller. Join its Wi-Fi and try again.');
    }
    final text = res.body.trim();
    if (res.statusCode == 200) return text;
    final msg = text.startsWith('ERR,') ? text.substring(4) : 'Error ${res.statusCode}';
    if (res.statusCode == 401 && _token != null && path != '/api/login' && path != '/api/provision') {
      _token = null;
      onUnauthorized?.call();
    }
    throw ApiException(msg, statusCode: res.statusCode);
  }

  /// Reads the controller's Wi-Fi status. Needs no login.
  Future<DeviceInfo> info({String? host}) async {
    final saved = _host;
    if (host != null) this.host = host;
    try {
      final text = await _post('/api/info', const {}, timeout: const Duration(seconds: 3));
      return DeviceInfo.fromJson(jsonDecode(text) as Map<String, dynamic>);
    } finally {
      if (host != null) _host = saved;
    }
  }

  /// Sends the site Wi-Fi to the controller. Works without a login over the
  /// device AP; from the site Wi-Fi it needs a login.
  Future<void> provision(String ssid, String pass) async {
    final t = _token;
    await _post('/api/provision', {'ssid': ssid, 'pass': pass, if (t != null) 't': t});
  }

  Future<void> login(String user, String pass) async {
    final text = await _post('/api/login', {'user': user, 'pass': pass});
    final token = (jsonDecode(text) as Map<String, dynamic>)['token'] as String?;
    if (token == null || token.isEmpty) throw ApiException('Unexpected reply from the controller.');
    _token = token;
  }

  Future<void> logout() async {
    final t = _token;
    _token = null;
    if (t == null) return;
    try {
      await _post('/api/logout', {'t': t}, timeout: const Duration(seconds: 2));
    } catch (_) {
      // Signing out locally is enough if the controller is out of reach.
    }
  }

  Map<String, String> _auth([Map<String, String> extra = const {}]) {
    final t = _token;
    if (t == null) throw ApiException('Sign in first.', statusCode: 401);
    return {'t': t, ...extra};
  }

  Future<String> command(String line) => _post('/api/cmd', _auth({'c': line}));

  Future<String> status() => _post('/api/status', _auth());

  Future<void> changeAccount({required String oldPass, required String user, required String pass}) async {
    await _post('/api/account', _auth({'old': oldPass, 'user': user, 'pass': pass}));
    _token = null; // the controller asks for a new sign-in
  }

  /// Changes the device AP name and password (the controller restarts).
  Future<void> changeAp({required String ssid, required String pass}) async {
    await _post('/api/wifi', _auth({'ssid': ssid, 'pass': pass}));
    _token = null; // the controller restarts
  }
}

/// Sends commands one at a time. While a request is in flight, newer
/// commands of the same kind replace older ones, so dragging a slider or
/// the color wheel never floods the controller.
class CommandQueue {
  CommandQueue(this.api, {this.onReply, this.onError});

  final ControllerApi api;
  final void Function(String reply)? onReply;
  final void Function(ApiException e)? onError;

  final Map<String, String> _pending = {};
  bool _busy = false;

  void send(String key, String line) {
    _pending[key] = line;
    _pump();
  }

  Future<void> _pump() async {
    if (_busy) return;
    _busy = true;
    try {
      while (_pending.isNotEmpty) {
        final key = _pending.keys.first;
        final line = _pending.remove(key)!;
        try {
          final reply = await api.command(line);
          onReply?.call(reply);
        } on ApiException catch (e) {
          onError?.call(e);
          if (e.isUnauthorized) {
            _pending.clear();
            break;
          }
        }
        await Future<void>.delayed(const Duration(milliseconds: 40));
      }
    } finally {
      _busy = false;
    }
  }
}
