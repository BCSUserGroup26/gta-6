# GTA6 BJB

Fresh native PS5 menu project for PPSA99991. Runtime rendering uses the public native VideoOut canvas; video uses the included MIT-licensed pl_mpeg decoder, with separately prepared PCM played through AudioOut. SDL and FFmpeg are not linked into the application. FFmpeg is a build-time conversion tool.

## Build from a new GitHub repository

1. Extract this ZIP. Upload the **contents** of `gta6-bjb`, with `src`, `media`, `prepare.py` and `LICENSE` at the repository root. All individual media files are below 25 MB.
2. Make sure the workflow is at `.github/workflows/build.yml`. If your uploader omits hidden folders, create that path and paste in the supplied YAML.
3. Open Actions → Build GTA6 BJB → Run workflow.
4. Download `GTA6-BJB-PPSA99991` from the completed run. Extract the artifact ZIP, then extract the application ZIP inside it.
5. Use the resulting `PPSA99991` directory with your existing directory-app launch method. Back up and replace the previous directory with this ID; do not mix old and new executable/runtime files. This workflow produces a folder application ZIP, not an installer PKG.

## Sequence

- On the system splash: first requested notification, wait 2 seconds, second requested notification, wait 2 seconds. These are cosmetic messages only.
- Thanks for using GTA6 BJB is displayed for 2 seconds, followed by `media/loading.mp4` (the supplied new_loading.mp4).
- Menu background fades in over 1 second. One second after it settles, the separate logo starts a 1-second fade and menu music starts. Options appear one second after the logo starts.
- The layout follows the supplied concept: options are centered in their left panel, logo is centered in the right area. The selected option is enlarged and yellow. Text uses DejaVu Sans; notification text uses the system font.
- D-pad up/down or vertical movement on either stick moves selection. Cross chooses. Circle returns from Settings. Settings lets you toggle menu music. Startup and video sequences cannot be skipped with controller buttons.
- New Game fades the menu and its music out over 0.6 seconds, then plays `loop.mp4` twice from the beginning.
- The last loading frame fades to black over 2 seconds. The centered logo fades in over 5 seconds while smoothly growing. It stays settled for 1 second, then plays the final supplied video. After the video ends, the menu returns.

`hover_theme.m4a` loops as main-menu music. It is not converted into launcher ATRAC9 hover audio by this workflow; that requires a separate ATRAC9 encoder. `logo.png` supplies both the app icon and independently animated UI logos. Launcher backgrounds use the two supplied artwork files.

Video is converted to 640×360 at 25 fps to reduce CPU decoding cost, then scaled to the display. The source loop has been compressed for browser upload while preserving its duration and audio. Original media outside this project has not been changed.

## Validation and limitations

Host C++ syntax checks passed. All three converted streams were fully decoded with the included decoder: 777 intro frames, 1052 loading-loop frames and 280 final-video frames. The media preparation script completed locally. The PS5 compiler and final hardware launch were not available in this environment, so a successful Actions build and testing on your console are still required. This is not a confirmed fix for the earlier startup crash. Notifications can remain visible longer than their submission gap because the shell controls their display duration.

Sources: https://github.com/blackbearreloaded/ps5-native-app-boilerplate (pinned 3ada439fa044f60c8b488678577a740761385711), https://github.com/phoboslab/pl_mpeg. The renderer is GPL-3.0-or-later; pl_mpeg's MIT license is included in its header. DejaVu fonts are provided by the build host.
