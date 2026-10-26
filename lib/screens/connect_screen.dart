import 'dart:async';

import 'package:flutter/material.dart';
import 'package:wifi_iot/wifi_iot.dart';

import '../api.dart';
import '../main.dart';
import '../storage.dart';
import 'home_screen.dart';

/// The first screen, and the device profile screen (from the avatar on Home).
///
///   1. Connect to the device AP
///   2. Connect the device to the site Wi-Fi
///   3. IP address of the device
///   4. User login (or, when signed in: account settings and sign out)
class ConnectScreen extends StatefulWidget {
  const ConnectScreen({super.key, this.message, this.profile = false});

  final String? message;

  /// true when opened from Home: shows account settings instead of the login.
  final bool profile;

  @override
  State<ConnectScreen> createState() => _ConnectScreenState();
}

class _ConnectScreenState extends State<ConnectScreen> {
  final _apSsid = TextEditingController();
  final _apPass = TextEditingController();
  final _siteSsid = TextEditingController();
  final _sitePass = TextEditingController();
  final _host = TextEditingController();
  final _user = TextEditingController();
  final _pass = TextEditingController();
  // profile mode
  final _oldPass = TextEditingController();
  final _newUser = TextEditingController();
  final _newPass = TextEditingController();
  final _newApSsid = TextEditingController();
  final _newApPass = TextEditingController();

  DeviceInfo? _info;
  bool _probing = false;
  bool _joiningAp = false;
  bool _provisioning = false;
  bool _signingIn = false;
  bool _showPass = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _error = widget.message;
    _load();
  }

  Future<void> _load() async {
    final (apSsid, apPass) = await AppStorage.wifi();
    _apSsid.text = apSsid;
    _apPass.text = apPass;
    _newApSsid.text = apSsid;
    _siteSsid.text = await AppStorage.siteSsid();
    _host.text = widget.profile ? controllerApi.host : (await AppStorage.host() ?? ControllerApi.apHost);
    _user.text = await AppStorage.lastUser();
    _newUser.text = _user.text;
    if (mounted) setState(() {});
    await _probe();
  }

  @override
  void dispose() {
    for (final c in [
      _apSsid, _apPass, _siteSsid, _sitePass, _host, _user, _pass,
      _oldPass, _newUser, _newPass, _newApSsid, _newApPass,
    ]) {
      c.dispose();
    }
    super.dispose();
  }

  void _toast(String msg) => ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));

  /// Looks for the device: first at the IP address field, then on the device AP.
  Future<String?> _probe() async {
    setState(() => _probing = true);
    String? found;
    final candidates = {_host.text.trim(), ControllerApi.apHost}..removeWhere((h) => h.isEmpty);
    for (final h in candidates) {
      try {
        _info = await controllerApi.info(host: h);
        found = h;
        break;
      } on ApiException {
        _info = null;
      }
    }
    if (mounted) {
      setState(() {
        _probing = false;
        // Fill the IP address once the device is on the site Wi-Fi.
        final ip = _info?.staIp ?? '';
        if (ip.isNotEmpty && (_host.text.isEmpty || _host.text == ControllerApi.apHost)) _host.text = ip;
        if (_info != null && _siteSsid.text.isEmpty) _siteSsid.text = _info!.staSsid;
      });
    }
    return found;
  }

  // ---- 1. Device AP ----
  Future<void> _joinAp() async {
    setState(() {
      _joiningAp = true;
      _error = null;
    });
    try {
      final ok = await WiFiForIoTPlugin.connect(
        _apSsid.text.trim(),
        password: _apPass.text,
        security: NetworkSecurity.WPA,
        joinOnce: true,
        withInternet: false,
      );
      if (ok) {
        await WiFiForIoTPlugin.forceWifiUsage(true);
        await AppStorage.setWifi(_apSsid.text.trim(), _apPass.text);
        await Future<void>.delayed(const Duration(seconds: 1));
      } else {
        _error = 'Could not join ${_apSsid.text}. Check the AP name and password, or join it from Android Settings.';
      }
    } catch (_) {
      _error = 'Could not join automatically. Join ${_apSsid.text} from Android Settings.';
    }
    if (mounted) setState(() => _joiningAp = false);
    await _probe();
  }

  // ---- 2. Device to site Wi-Fi ----
  Future<void> _provision() async {
    final ssid = _siteSsid.text.trim();
    if (ssid.isEmpty) { _toast('Enter the Wi-Fi name (SSID).'); return; }
    final reach = await _probe();
    if (reach == null) { _toast('Connect to the device AP first.'); return; }
    setState(() {
      _provisioning = true;
      _error = null;
    });
    final saved = controllerApi.host;
    controllerApi.host = reach;
    try {
      await controllerApi.provision(ssid, _sitePass.text);
      await AppStorage.setSiteSsid(ssid);
      // The device AP can drop for a moment while the ESP32 joins; keep asking.
      for (var i = 0; i < 20; i++) {
        await Future<void>.delayed(const Duration(milliseconds: 1500));
        try {
          final info = await controllerApi.info(host: ControllerApi.apHost);
          if (mounted) setState(() => _info = info);
          if (info.siteConnected) {
            _host.text = info.staIp;
            break;
          }
          if (info.staStatus == 'failed' || info.staStatus == 'not_found') break;
        } on ApiException {
          // AP briefly unavailable; try again
        }
      }
    } on ApiException catch (e) {
      _error = e.isUnauthorized ? 'Connect over the device AP, or sign in first, to change the Wi-Fi.' : e.message;
    } finally {
      controllerApi.host = saved;
      if (mounted) setState(() => _provisioning = false);
    }
  }

  /// Leave the device AP so the phone goes back to the site Wi-Fi.
  Future<void> _switchPhoneToSite() async {
    try {
      await WiFiForIoTPlugin.forceWifiUsage(false);
      await WiFiForIoTPlugin.disconnect();
    } catch (_) {}
    _toast('Switching the phone to ${_info?.staSsid ?? 'your Wi-Fi'}...');
    await Future<void>.delayed(const Duration(seconds: 4));
    await _probe();
  }

  // ---- 4. Sign in ----
  Future<void> _signIn() async {
    final user = _user.text.trim();
    if (user.isEmpty || _pass.text.isEmpty) {
      setState(() => _error = 'Enter your username and password.');
      return;
    }
    setState(() {
      _signingIn = true;
      _error = null;
    });
    final host = await _probe();
    if (host == null) {
      setState(() {
        _signingIn = false;
        _error = 'Device not found at ${_host.text}. Connect to the device AP or check the IP address.';
      });
      return;
    }
    try {
      // On the device AP, keep traffic on it even though it has no internet.
      await WiFiForIoTPlugin.forceWifiUsage(host == ControllerApi.apHost);
    } catch (_) {}
    try {
      controllerApi.host = host;
      await controllerApi.login(user, _pass.text);
      await AppStorage.setLastUser(user);
      await AppStorage.setHost(host);
      if (!mounted) return;
      Navigator.of(context).pushAndRemoveUntil(MaterialPageRoute(builder: (_) => const HomeScreen()), (_) => false);
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      if (mounted) setState(() => _signingIn = false);
    }
  }

  // ---- Profile mode ----
  void _backToLogin(String message) {
    Navigator.of(context).pushAndRemoveUntil(
      MaterialPageRoute(builder: (_) => ConnectScreen(message: message)),
      (_) => false,
    );
  }

  Future<void> _changeLogin() async {
    if (_newUser.text.trim().length < 3) { _toast('Username needs 3 or more characters.'); return; }
    if (_newPass.text.length < 6) { _toast('Password needs 6 or more characters.'); return; }
    try {
      await controllerApi.changeAccount(oldPass: _oldPass.text, user: _newUser.text.trim(), pass: _newPass.text);
      await AppStorage.setLastUser(_newUser.text.trim());
      if (mounted) _backToLogin('Login changed. Sign in with the new details.');
    } on ApiException catch (e) {
      _toast(e.message);
    }
  }

  Future<void> _changeAp() async {
    final ssid = _newApSsid.text.trim();
    if (ssid.isEmpty || ssid.length > 32) { _toast('AP name needs 1 to 32 characters.'); return; }
    if (_newApPass.text.length < 8) { _toast('AP password needs 8 or more characters.'); return; }
    try {
      await controllerApi.changeAp(ssid: ssid, pass: _newApPass.text);
      await AppStorage.setWifi(ssid, _newApPass.text);
      if (mounted) _backToLogin('Device restarted with AP "$ssid". Sign in again.');
    } on ApiException catch (e) {
      _toast(e.message);
    }
  }

  Future<void> _signOut() async {
    await controllerApi.logout();
    if (mounted) _backToLogin('Signed out.');
  }

  // ---- UI ----
  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final text = Theme.of(context).textTheme;
    final info = _info;
    const gap = SizedBox(height: 12);
    InputDecoration deco(String label, {IconData? icon, Widget? suffix}) => InputDecoration(
          labelText: label,
          prefixIcon: icon == null ? null : Icon(icon),
          suffixIcon: suffix,
          border: const OutlineInputBorder(),
          isDense: true,
        );

    // Status lines
    final String apStatus;
    final bool apOk;
    if (info != null && info.viaAp) {
      apStatus = 'Connected to ${info.apSsid}';
      apOk = true;
    } else if (info != null) {
      apStatus = 'Not needed: reaching the device over ${info.staSsid}';
      apOk = true;
    } else {
      apStatus = 'Not connected';
      apOk = false;
    }

    final String siteStatus;
    final bool siteOk = info?.siteConnected ?? false;
    if (info == null) {
      siteStatus = 'Connect to the device first';
    } else {
      siteStatus = switch (info.staStatus) {
        'connected' => 'Connected to ${info.staSsid}',
        'connecting' => 'Connecting to ${info.staSsid}...',
        'failed' => 'Could not join ${info.staSsid}. Check the password.',
        'not_found' => '${info.staSsid} not found. Check the name and range.',
        _ => 'Not set up',
      };
    }

    return Scaffold(
      appBar: AppBar(
        title: Text(widget.profile ? 'Device profile' : 'MORAG Lighting'),
        actions: [
          IconButton(
            tooltip: 'Refresh',
            onPressed: _probing ? null : _probe,
            icon: _probing
                ? const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.refresh),
          ),
        ],
      ),
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 480),
            child: ListView(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 32),
              children: [
                // 1. Device AP
                _StepCard(
                  step: '1',
                  title: 'Connect to device AP',
                  status: apStatus,
                  ok: apOk,
                  children: [
                    ExpansionTile(
                      tilePadding: EdgeInsets.zero,
                      title: Text('AP: ${_apSsid.text}', style: text.bodyMedium),
                      childrenPadding: const EdgeInsets.only(bottom: 8),
                      children: [
                        TextField(controller: _apSsid, decoration: deco('Device AP name')),
                        gap,
                        TextField(controller: _apPass, obscureText: true, decoration: deco('Device AP password')),
                      ],
                    ),
                    FilledButton.tonalIcon(
                      onPressed: _joiningAp ? null : _joinAp,
                      icon: _joiningAp ? const _Spinner() : const Icon(Icons.wifi_tethering),
                      label: const Text('Connect device AP'),
                    ),
                  ],
                ),
                gap,

                // 2. Device to site Wi-Fi
                _StepCard(
                  step: '2',
                  title: 'Connect device to Wi-Fi',
                  status: siteStatus,
                  ok: siteOk,
                  children: [
                    TextField(controller: _siteSsid, decoration: deco('SSID', icon: Icons.wifi)),
                    gap,
                    TextField(
                      controller: _sitePass,
                      obscureText: true,
                      decoration: deco('Password', icon: Icons.key),
                    ),
                    gap,
                    FilledButton.tonalIcon(
                      onPressed: _provisioning ? null : _provision,
                      icon: _provisioning ? const _Spinner() : const Icon(Icons.wifi),
                      label: Text(_provisioning ? 'Connecting...' : 'Connect to Wi-Fi'),
                    ),
                    if (siteOk && (info?.viaAp ?? false)) ...[
                      const SizedBox(height: 8),
                      OutlinedButton.icon(
                        onPressed: _switchPhoneToSite,
                        icon: const Icon(Icons.swap_horiz),
                        label: Text('Switch phone to ${info!.staSsid}'),
                      ),
                    ],
                  ],
                ),
                gap,

                // 3. IP address
                TextField(
                  controller: _host,
                  keyboardType: TextInputType.url,
                  decoration: deco('IP address', icon: Icons.lan_outlined).copyWith(
                    helperText: siteOk
                        ? 'Device on ${info!.staSsid}: ${info!.staIp}. On the device AP: ${ControllerApi.apHost}'
                        : 'On the device AP the address is ${ControllerApi.apHost}',
                  ),
                  onSubmitted: (_) => _probe(),
                ),
                const SizedBox(height: 16),

                // 4. Login / account
                if (!widget.profile)
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Text('User login', style: text.titleMedium, textAlign: TextAlign.center),
                          gap,
                          TextField(
                            controller: _user,
                            textInputAction: TextInputAction.next,
                            autofillHints: const [AutofillHints.username],
                            decoration: deco('Username', icon: Icons.person_outline),
                          ),
                          gap,
                          TextField(
                            controller: _pass,
                            obscureText: !_showPass,
                            autofillHints: const [AutofillHints.password],
                            onSubmitted: (_) => _signIn(),
                            decoration: deco(
                              'Password',
                              icon: Icons.lock_outline,
                              suffix: IconButton(
                                icon: Icon(_showPass ? Icons.visibility_off : Icons.visibility),
                                onPressed: () => setState(() => _showPass = !_showPass),
                              ),
                            ),
                          ),
                          if (_error != null) ...[
                            gap,
                            Text(_error!, style: TextStyle(color: scheme.error)),
                          ],
                          const SizedBox(height: 16),
                          FilledButton(
                            onPressed: _signingIn ? null : _signIn,
                            style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(48)),
                            child: _signingIn ? const _Spinner() : const Text('Sign in'),
                          ),
                        ],
                      ),
                    ),
                  )
                else
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          ListTile(
                            contentPadding: EdgeInsets.zero,
                            leading: CircleAvatar(child: Text(_user.text.isEmpty ? '?' : _user.text[0].toUpperCase())),
                            title: Text('Signed in as ${_user.text}'),
                            subtitle: Text('Device at ${controllerApi.host}'),
                          ),
                          if (_error != null) Text(_error!, style: TextStyle(color: scheme.error)),
                          ExpansionTile(
                            tilePadding: EdgeInsets.zero,
                            leading: const Icon(Icons.manage_accounts_outlined),
                            title: const Text('Change login'),
                            childrenPadding: const EdgeInsets.only(bottom: 12),
                            children: [
                              TextField(controller: _oldPass, obscureText: true, decoration: deco('Current password')),
                              gap,
                              TextField(controller: _newUser, decoration: deco('New username')),
                              gap,
                              TextField(controller: _newPass, obscureText: true, decoration: deco('New password (6+)')),
                              gap,
                              FilledButton(onPressed: _changeLogin, child: const Text('Save login')),
                            ],
                          ),
                          ExpansionTile(
                            tilePadding: EdgeInsets.zero,
                            leading: const Icon(Icons.wifi_tethering),
                            title: const Text('Device AP name and password'),
                            childrenPadding: const EdgeInsets.only(bottom: 12),
                            children: [
                              TextField(controller: _newApSsid, decoration: deco('AP name')),
                              gap,
                              TextField(controller: _newApPass, obscureText: true, decoration: deco('AP password (8+)')),
                              gap,
                              FilledButton.tonal(onPressed: _changeAp, child: const Text('Save and restart device')),
                            ],
                          ),
                          gap,
                          OutlinedButton.icon(
                            onPressed: _signOut,
                            icon: const Icon(Icons.logout),
                            label: const Text('Sign out'),
                          ),
                          gap,
                          Text(
                            'Forgot the login? While the device is on, hold its BOOT button for 5 seconds. '
                            'It flashes white and restarts with the default AP and login.',
                            style: TextStyle(color: scheme.onSurfaceVariant, fontSize: 12),
                          ),
                        ],
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _StepCard extends StatelessWidget {
  const _StepCard({required this.step, required this.title, required this.status, required this.ok, required this.children});
  final String step;
  final String title;
  final String status;
  final bool ok;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                CircleAvatar(
                  radius: 14,
                  backgroundColor: ok ? scheme.primary : scheme.surfaceContainerHighest,
                  foregroundColor: ok ? scheme.onPrimary : scheme.onSurfaceVariant,
                  child: ok ? const Icon(Icons.check, size: 16) : Text(step, style: const TextStyle(fontSize: 13)),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(title, style: const TextStyle(fontWeight: FontWeight.w600)),
                      Text(status, style: TextStyle(color: ok ? scheme.primary : scheme.onSurfaceVariant, fontSize: 13)),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            ...children,
          ],
        ),
      ),
    );
  }
}

class _Spinner extends StatelessWidget {
  const _Spinner();
  @override
  Widget build(BuildContext context) =>
      const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2));
}