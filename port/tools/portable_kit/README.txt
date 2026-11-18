Super Mario 64 DS - PC port
===========================

This is a work-in-progress PC build of Super Mario 64 DS, made from a
decompilation of the game. It is the real game code running natively on your
PC: the real physics, the real camera, the real Mario.

It contains no game data at all. No models, no textures, no music, no levels.
All of that comes out of a Super Mario 64 DS cartridge that you own.


WHAT YOU NEED
-------------

1. A Windows PC (Windows 10 or 11). Nothing to install.

2. Your own copy of Super Mario 64 DS for the Nintendo DS, and a dump of that
   cartridge saved as a .nds file. Dumping a cartridge you own is done with a
   DS and a dumping tool; how to do that is outside this kit. A copy
   downloaded from the internet is not the same thing and is not supported
   here.

   Only the European cartridge works for now. A North American or Japanese
   dump is a different build, and the launcher tells you so.


HOW TO PLAY
-----------

1. Copy your .nds file into the folder named PLACE YOUR ROM HERE.

2. Double-click SM64DSLauncher.exe and press Play.

   The first time you do this it unpacks the game data out of your dump. That
   takes about a minute and creates two folders here, "extracted" and
   "build". It only happens once; after that Play starts the game straight
   away.


4. If you want the game to start without the launcher, tick Skip launcher in
   Settings. Hold Shift while it starts to open the launcher again.

5. That is all.


PLAYING WITH OTHER PEOPLE (open beta)
-------------------------------------

Multiplayer is an open beta in 0.9.3. Expect rough edges, and please tell us
what breaks. Nobody needs to change any setting on their router, and nothing
needs an account.

Everyone in a room has to run the same build. If someone is on another
version they are turned away, and the launcher says why.

1. Open the launcher and click Multiplayer. A window opens.

2. Type a name for yourself under Your name. My colours picks the colours of
   your Yoshi and its shoes.




   A room gives you a six letter code. A friend types it into the Room code
   box and presses Join. The code is not case sensitive and only works while
   somebody is still in the room. The host can remove a player with the
   Remove player button, and that player cannot come back to the room for a
   couple of minutes.




To report a problem while playing, press F6 (the launcher keeps running
behind the game). A small box asks what happened. If bug reports are off in
Settings it asks before it sends. Also useful: the newest play_*.log in the
playlog folder next to the game, from each player, with the room type and
who hosted.


CONTROLS
--------

Xbox controller:

    Left stick          walk
    A                   jump
    X                   run
    B                   punch
    Y                   first-person look (press again to go back)
    Right trigger       crouch
    Left trigger        put the camera back behind Mario
    Right stick         turn the camera (the harder you push, the faster)
    Bumpers             turn the DS camera one step left or right (in the
                        chase and free cameras they zoom out and in)
    Right stick click   same as F1 below
    Start               the game's own Start button (pause)

Keyboard and mouse:


The camera starts as DS stepped, the cartridge's own camera. F1 below
changes it.

Extra keys:


Every camera action can be put on a key, a pad button or a mouse button
(Settings, Controls). The list has Rotate camera left and right, Tilt camera
up and down, Zoom camera in and out, and Center camera. Zoom is not bound to
anything until you pick a key. Right stick zooms makes the right stick zoom
the DS camera instead of turning it. Camera speed also changes how fast the
DS camera turns. The camera binds and sticks do nothing while the game is
paused.

A controller works in every menu: A confirms and B goes back. On the face
pages and in the minigames the right stick moves a small pointer, A taps
and Start pauses.

First-person look (Y and Z above) can be rebound like the other buttons,
under Controls in the launcher's Settings.

Frame rate is on the General page of the launcher's Settings. Native is the
DS's own speed. Pick 60 or more and the game draws extra pictures in
between, so motion looks smooth. It takes effect the next time the game
starts.


NEW SETTINGS IN 0.9.3
---------------------

These are in the launcher's Settings. The ones marked (game) are also on the
Settings page inside the game: open the pause menu's options, or press the
wrench on the title screen.

    Start fullscreen         General page. Starts the game in borderless
                             fullscreen. Off by default.




    Character select on      General page. Shows a row of character heads
    star select (game)       before the course on the star select screen.
                             On by default. Off keeps the DS caps row.

    Skip launcher            General page. Starts the game straight away
                             after a short Starting window. Hold Shift while
                             it starts to open the launcher. Off by default.

    add to Steam             General page button. Adds the launcher to Steam
                             as a shortcut called SM64DS Port. Close Steam
                             first.




WHAT YOU CAN DO IN IT
---------------------


This is a pre-1.0 release. Some things are still rough, and some levels are
further along than others.


CREDITS
-------

Testing: thunderit tested every round of 0.9.3, rounds 1 to 6.



LINUX AND STEAM DECK (Proton)
-----------------------------

This is a Windows program. It is not built for Linux, and it is not supported
there. One player has reported it running well under Steam's Proton
Experimental (Arch, KDE, Wayland, NVIDIA). That is the only setup anyone has
confirmed. Other Proton builds (GE, dwproton) and plain Wine have not been
confirmed to work, and a Steam Deck has not been tested. If you try it, please
tell us how it went.

What is different from Windows: the launcher unpacks your dump with Windows
PowerShell, and a Proton prefix does not have one. So the unpacking is done
once, outside Proton, with Python 3 (extract_assets.py, included; SteamOS and
most distros already have python3). It makes exactly the same files.

Easiest way, on a desktop with Steam installed:

1. Install "Proton Experimental" in Steam (Library, set the filter to Tools).
2. Copy your .nds file into the folder named PLACE YOUR ROM HERE.
3. Open a terminal in this folder and run:   bash launch.sh
   The first run unpacks the game data (about a minute), then opens the
   launcher inside Proton. Press Play there.

On a Steam Deck (or to start it from Steam):

1. Switch to Desktop Mode. Unzip the kit into your home folder (for example
   into Games), and put your .nds in PLACE YOUR ROM HERE.
2. Double-click "Steam Deck Setup" in the kit folder.
   The first time, KDE asks whether to run it, because files that come out
   of a zip are not marked as programs ("This will start the program: Steam
   Deck Setup"). Press Continue. It only asks once.
   A terminal window opens and the setup:
     - checks your .nds is in PLACE YOUR ROM HERE,
     - unpacks the game data (about a minute, only the first time),
     - asks to close Steam for a moment, adds "Super Mario 64 DS PC" to your
       Steam library set to run with Proton Experimental, and starts Steam
       again. Your old Steam files are kept next to the new ones with .bak-
       and the date at the end of their names.
   Running it again does no harm: it does not add a second entry.
3. Switch to Gaming Mode and find Super Mario 64 DS PC in your library.
   If the setup said Proton Experimental is not installed, install it first:
   Library, set the filter to Tools, Proton Experimental, Install.

If double-clicking does nothing or opens a text editor, open a terminal in the
kit folder (in Dolphin: right-click inside the folder, Open Terminal Here) and
run  python3 steam_deck_setup.py  instead. It is the same thing. A kit on a
memory card formatted as exFAT cannot be marked as a program, so there the
terminal is the way, or unzip the kit to the Deck's own drive.

The same steps by hand, if the setup stops or you prefer to do it yourself:

1. Switch to Desktop Mode, unzip the kit, put your .nds in PLACE YOUR ROM HERE.
2. Open a terminal in the kit folder and run  python3 extract_assets.py  once,
   so the game data is unpacked (about a minute).
3. In Steam choose Add a Non-Steam Game, browse to SM64DSLauncher.exe in the
   kit folder and add it.
4. Open the new entry's Properties, then Compatibility, tick "Force the use of
   a specific Steam Play compatibility tool" and pick Proton Experimental.
5. Run it from Steam, in Desktop Mode or Gaming Mode.

If the picture is black, wrong, or the game closes at start: run  bash
launch.sh --safe  (or set the launch option  SM64DS_RENDERER=0
SM64DS_PRESENT_BACKEND=0 SM64DS_PRESENTER=0 %command%  on the Steam entry). That
draws with the processor and the plain Windows present path, which does not
need Direct3D 11. Without --safe the game is meant to fall back to that by
itself when it cannot get a Direct3D 11 device.

Things to know:
- Updates: the launcher restarts itself after an update with a Windows
  PowerShell helper that Proton does not have. If it just closes, start it
  again (the update is applied on that start).
- Wayland: the one player who got it running did not say whether
  PROTON_ENABLE_WAYLAND mattered. Leave it unset unless X11 gives you trouble.
- To report a problem, run  bash launch.sh --log  and send the steam-*.log
  file from your home folder, plus what happened. Crash reports sent by the
  launcher are marked as coming from Wine, so we can tell them apart.


IF SOMETHING GOES WRONG
-----------------------

"No .nds file found"
    Your dump is not here yet. Put it in the PLACE YOUR ROM HERE folder. Its
    name has to end in .nds.

"That is not a Super Mario 64 DS dump"
    The .nds file here is a different game, or the dump is damaged.

"This is the US version. The PC port needs the European cartridge for now."
    Your dump is another region's (the message names it). Only the European
    cartridge works for now.

"This dump is truncated" or "incomplete"
    The dump did not finish. Dump the cartridge again.

"...but not a revision this build knows"
    The cartridge is a Super Mario 64 DS release this build has not been
    matched against. Nothing to be done from here.

The game window opens and closes immediately
    Start it from the launcher rather than the exe so the message stays on
    screen. If it keeps happening, the launcher's crash report is the fastest
    way for it to get fixed.


WHAT IS IN THIS FOLDER
----------------------


The rest are small files the launcher and the game read. Leave them as they
are: version.json, launcher_config.json, romdata.manifest and
romdata.recipe.tsv.

Once you have run it, more appear, all made on your PC:


