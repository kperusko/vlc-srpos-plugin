Plugin module for VLC player
============================
This is a fork with "fixes" that allows the plugin to compile.
The original source code can be found at http://vlcsrposplugin.sourceforge.net/

INSTALLATION
============

Required packages:
  * libvlc-dev 
  * libvlccore-dev

Build and install:
	  	  ./configure && make && make install


VLC PLAYER SETUP
================

1. Start VLC player;
2. Open preferences window (menu Tools->Preferences);
3. Select 'All' in 'Show settings';
4. Open Control interfaces panel (Interface->Control interfaces);
5. Check 'Save/restore position of the last played files' checkbox;
6. Click 'Save' button;
7. Restart VLC player.

