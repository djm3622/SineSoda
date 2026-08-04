# Install SineSoda in FL Studio

SineSoda is a VST3 effect. Download the archive for your operating system from
the [latest GitHub release](https://github.com/djm3622/SineSoda/releases/latest),
extract it, and keep the complete `SineSoda.vst3` bundle intact.

## macOS

The public macOS archive contains a universal arm64 and x86_64 VST3 bundle.
Copy it to:

```text
~/Library/Audio/Plug-Ins/VST3/SineSoda.vst3
```

Version 1.0.0 is ad-hoc signed but not Apple-notarized. If macOS quarantines the
download, first try opening FL Studio and approving the plug-in in **System
Settings > Privacy & Security**. If macOS still blocks it, remove quarantine
from this specific installed bundle in Terminal:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/SineSoda.vst3
```

Do not run that command on a broader directory.

## Windows

The public Windows archive contains a 64-bit VST3 bundle. Copy it to the common
per-user VST3 location:

```text
%LOCALAPPDATA%\Programs\Common\VST3\SineSoda.vst3
```

Alternatively, copy it to a custom VST3 directory already listed in FL
Studio's Plugin Manager. If Windows marks the ZIP as downloaded from another
computer, right-click the ZIP, choose **Properties**, select **Unblock**, and
extract it again before copying the bundle.

## Scan and use the plug-in

1. In FL Studio, open **Options > Manage plugins**.
2. Make sure the directory containing `SineSoda.vst3` is in the search paths.
3. Select **Find installed plugins**.
4. Add SineSoda to a mixer effect slot. It is an effect, not a generator.

If an older build is already installed, close FL Studio before replacing the
bundle, then rescan after restarting it.
