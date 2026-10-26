import 'package:flutter/material.dart';

import 'api.dart';
import 'screens/connect_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const MoragApp());
}

/// One controller link for the whole app.
final ControllerApi controllerApi = ControllerApi();
final GlobalKey<NavigatorState> navigatorKey = GlobalKey<NavigatorState>();

class MoragApp extends StatefulWidget {
  const MoragApp({super.key});

  @override
  State<MoragApp> createState() => _MoragAppState();
}

class _MoragAppState extends State<MoragApp> {
  @override
  void initState() {
    super.initState();
    // When the controller says the login expired, go back to sign-in.
    controllerApi.onUnauthorized = () {
      navigatorKey.currentState?.pushAndRemoveUntil(
        MaterialPageRoute(builder: (_) => const ConnectScreen(message: 'Your session ended. Sign in again.')),
        (_) => false,
      );
    };
  }

  @override
  Widget build(BuildContext context) {
    const seed = Color(0xFF6C3BD1);
    return MaterialApp(
      title: 'MORAG Lighting',
      debugShowCheckedModeBanner: false,
      navigatorKey: navigatorKey,
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(seedColor: seed),
      ),
      darkTheme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(seedColor: seed, brightness: Brightness.dark),
      ),
      home: const ConnectScreen(),
    );
  }
}
