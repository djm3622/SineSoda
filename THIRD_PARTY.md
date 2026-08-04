# Third-party notices

SineSoda source is Apache-2.0. The VST3 target links the following permissively
licensed source dependencies at build time.

## iPlug2

- Source: <https://github.com/iPlug2/iPlug2>
- Pinned commit: `5c2df9dce3f5258acfeff3846a6a9563f382212c`
- License: zlib-style; copyright the iPlug2 developers and contributors
- Upstream notice: `_deps/iplug2-src/LICENSE.txt` after CMake population

iPlug2 incorporates or can optionally use additional permissive components.
SineSoda's Metal/NanoVG UI build uses the subset selected by iPlug2's CMake
target; retain the corresponding upstream source notices when redistributing
source or binaries.

## Steinberg VST3 SDK

- Source: <https://github.com/steinbergmedia/vst3sdk>
- Version: 3.8.0 build 66
- Pinned commit: `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`
- License: MIT
- Copyright: 2025 Steinberg Media Technologies GmbH
- Upstream notice:
  `_deps/vst3sdk-src/LICENSE.txt` after CMake population

The VST3 Usage Guidelines distributed by Steinberg also govern use of the VST
trademarks and logo. SineSoda does not include Steinberg sample plug-ins or
VSTGUI.

## Roboto

- Source: the `Roboto-Regular.ttf` resource distributed with the pinned iPlug2
  examples, originally published by Google
- License: Apache-2.0
- Copyright: Google and the Roboto authors

The font is embedded in the VST3 bundle so text rendering does not depend on
fonts installed on the host system.
