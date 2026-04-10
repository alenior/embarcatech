import 'package:flutter/material.dart';
import 'features/status/status_page.dart';
import 'features/history/history_page.dart';
import 'features/control/control_page.dart';

class ProjetoFinalApp extends StatelessWidget {
  const ProjetoFinalApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Projeto Final',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(useMaterial3: true, colorSchemeSeed: Colors.indigo),
      home: const MainTabsPage(),
    );
  }
}

class MainTabsPage extends StatefulWidget {
  const MainTabsPage({super.key});

  @override
  State<MainTabsPage> createState() => _MainTabsPageState();
}

class _MainTabsPageState extends State<MainTabsPage> {
  int _index = 0;

  final _pages = const [StatusPage(), HistoryPage(), ControlPage()];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _pages[_index],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (i) => setState(() => _index = i),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.shield), label: 'Status'),
          NavigationDestination(icon: Icon(Icons.history), label: 'Histórico'),
          NavigationDestination(
            icon: Icon(Icons.settings_remote),
            label: 'Controle',
          ),
        ],
      ),
    );
  }
}
