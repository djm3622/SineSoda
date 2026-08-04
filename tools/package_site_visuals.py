#!/usr/bin/env python3
"""Package repo-rendered analyzer frames in the captured FL Studio plugin UI."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence

from PIL import Image, ImageDraw, ImageFont


ROBOTO_PATH = Path("resources/fonts/Roboto-Regular.ttf")
FEATURE_FRAME_DURATION_MS = 50
TUTORIAL_FPS = 30
FEATURE_OUTPUT_WIDTH = 1000
FEATURE_PIANO_HEIGHT = 300
TUTORIAL_OUTPUT_WIDTH = 900
TUTORIAL_PIANO_HEIGHT = 270
TRANSFORM_STEP_FRAMES = TUTORIAL_FPS * 5
SCALE_STEP_FRAMES = TUTORIAL_FPS * 4
PLAYHEAD_PERIOD_FRAMES = TUTORIAL_FPS * 8
TRANSFORM_MOVE_END = round(TUTORIAL_FPS * 0.8)
TRANSFORM_CHANGE_FRAME = round(TUTORIAL_FPS * 0.9)
TRANSFORM_BLEND_END = round(TUTORIAL_FPS * 1.5)
PARAMETER_MOVE_START = round(TUTORIAL_FPS * 2.2)
PARAMETER_MOVE_END = round(TUTORIAL_FPS * 3.0)
PARAMETER_DRAG_START = round(TUTORIAL_FPS * 3.5)
PARAMETER_DRAG_END = round(TUTORIAL_FPS * 4.3)
PARAMETER_MENU_CHANGE = round(TUTORIAL_FPS * 3.6)
SCALE_MOVE_END = round(TUTORIAL_FPS * 0.8)
SCALE_CHANGE_FRAME = round(TUTORIAL_FPS * 0.9)
SCALE_BLEND_END = round(TUTORIAL_FPS * 1.6)
SCALE_RETURN_START = round(TUTORIAL_FPS * 2.4)
SCALE_RETURN_END = round(TUTORIAL_FPS * 3.2)
SCALE_DOMAIN_MOVE_END = round(TUTORIAL_FPS * 2.1)
SCALE_DOMAIN_FIRST_DRAG_END = round(TUTORIAL_FPS * 2.73)
SCALE_DOMAIN_SECOND_DRAG_END = round(TUTORIAL_FPS * 3.5)
ANALYSIS_FRAME_COUNT = 96
ANALYSIS_HOP_SIZE = 1024
ANALYSIS_SAMPLE_RATE = 48000.0
ANALYSIS_LOOP_SECONDS = ANALYSIS_FRAME_COUNT * ANALYSIS_HOP_SIZE / ANALYSIS_SAMPLE_RATE

# Native Retina capture geometry. The embedded editor is exactly 2200 x 1400,
# corresponding to the 1100 x 700 layout declared in config.h at 2x scale.
PLOT_RECT = (168, 494, 2060, 1334)
MENU_VALUE_CENTERS = {
    "transform": (214, 141),
    "fft_size": (574, 141),
    "bins_per_octave": (1294, 141),
    "display_scale": (2014, 141),
}
SLIDER_VALUE_CENTERS = {
    "bands": (682, 278),
    "q_scale": (1978, 278),
    "gamma": (304, 397),
}
SLIDER_TRACKS = {
    "bands": (497, 867, 239),
    "q_scale": (1793, 2166, 239),
    "gamma": (71, 537, 355),
}
SLIDER_RANGES = {
    "bands": (24.0, 512.0),
    "q_scale": (0.25, 4.0),
    "gamma": (0.0, 200.0),
}
SLIDER_DRAG_DISTANCE = 112
HEADER_TITLE_BOX = (70, 458, 780, 492)
CURSOR_POSITIONS = {
    "transform": (342, 154),
    "fft_size": (702, 154),
    "bins_per_octave": (1422, 154),
    "display_scale": (2142, 154),
}
SCALE_CURSOR_IDLE = (1730, 360)
SCALE_DOMAIN_CURSOR = (235, 914)
SCALE_DOMAIN_CURSOR_UP = (235, 650)
SCALE_DOMAIN_CURSOR_DOWN = (235, 1178)


@dataclass(frozen=True)
class TransformSpec:
    slug: str
    menu_label: str
    header_label: str
    parameter: str
    parameter_start: float
    parameter_end: float


@dataclass(frozen=True)
class Note:
    start: float
    duration: float
    midi_note: int
    velocity: float = 1.0


TRANSFORMS = (
    TransformSpec("stft", "STFT", "STFT", "fft_size", 1024.0, 8192.0),
    TransformSpec("log-filterbank", "Log filterbank", "Log filterbank", "bands", 24.0, 512.0),
    TransformSpec("mel", "Mel", "Mel", "bands", 512.0, 24.0),
    TransformSpec("bark", "Bark", "Bark", "bands", 24.0, 512.0),
    TransformSpec("erb", "ERB", "ERB", "bands", 512.0, 24.0),
    TransformSpec("cqt", "CQT", "CQT", "bins_per_octave", 12.0, 48.0),
    TransformSpec("vqt", "VQT", "VQT", "gamma", 0.0, 200.0),
    TransformSpec("morlet-cwt", "Morlet CWT", "Morlet CWT", "q_scale", 0.25, 4.0),
    TransformSpec("morse-cwt", "Morse CWT", "Morse CWT", "q_scale", 4.0, 0.25),
    TransformSpec("stockwell", "Stockwell", "Stockwell", "q_scale", 0.25, 4.0),
    TransformSpec(
        "reassigned-stft",
        "Reassigned STFT",
        "Reassigned STFT",
        "fft_size",
        8192.0,
        1024.0,
    ),
    TransformSpec(
        "synchrosqueezed-cwt",
        "Synchrosqueezed CWT",
        "Synchrosqueezed CWT",
        "q_scale",
        4.0,
        0.25,
    ),
    TransformSpec("chroma", "Chroma", "Chroma", "fft_size", 1024.0, 8192.0),
    TransformSpec(
        "haar-wavelet-packet",
        "Haar wavelet packet",
        "Haar wavelet packet",
        "bands",
        24.0,
        512.0,
    ),
    TransformSpec(
        "smoothed-pseudo-wigner-ville",
        "SPWVD",
        "Smoothed pseudo Wigner-Ville",
        "fft_size",
        8192.0,
        1024.0,
    ),
)
TRANSFORM_BY_SLUG = {spec.slug: spec for spec in TRANSFORMS}


def timeline_note(start: float, duration: float, midi_note: int, velocity: float) -> Note:
    """Convert the C++ renderer's loop seconds to the four-beat piano roll."""
    scale = 4.0 / ANALYSIS_LOOP_SECONDS
    return Note(start * scale, duration * scale, midi_note, velocity)


def harmonic_notes() -> tuple[Note, ...]:
    chords = (
        (36, 43, 48, 52, 59, 62),
        (38, 45, 48, 53, 57, 64),
        (29, 36, 41, 45, 52, 55),
        (31, 38, 41, 47, 52, 57),
    )
    chord_duration = max(ANALYSIS_LOOP_SECONDS, 1.0) / len(chords)
    return tuple(
        timeline_note(
            chord_duration * chord,
            chord_duration * 0.91,
            midi_note,
            0.58 + 0.07 * voice,
        )
        for chord, notes in enumerate(chords)
        for voice, midi_note in enumerate(notes)
    )


def pitch_structure_notes() -> tuple[Note, ...]:
    chords = (
        (48, 52, 55, 59, 62),
        (45, 48, 52, 55, 59),
        (41, 45, 48, 52, 55),
        (43, 47, 50, 53, 57),
    )
    step = max(ANALYSIS_LOOP_SECONDS, 1.0) / 20.0
    return tuple(
        timeline_note(
            step * (chord * 5 + voice),
            step * 1.42,
            midi_note,
            0.66 + 0.06 * ((voice + chord) % 4),
        )
        for chord, notes in enumerate(chords)
        for voice, midi_note in enumerate(notes)
    )


def transient_motion_notes() -> tuple[Note, ...]:
    step = max(ANALYSIS_LOOP_SECONDS, 1.0) / 24.0
    notes: list[Note] = []
    for index in range(24):
        ascending = 36 + index
        midi_note = ascending + 12 if index % 6 == 5 else ascending
        duration = step * (0.82 if index % 4 == 0 else 0.34)
        notes.append(
            timeline_note(
                step * index,
                duration,
                midi_note,
                1.0 if index % 3 == 0 else 0.72,
            )
        )
        if index % 6 == 0:
            notes.append(
                timeline_note(step * index, step * 0.18, midi_note + 19, 0.82)
            )
    return tuple(notes)


HARMONIC_NOTES = harmonic_notes()
PITCH_STRUCTURE_NOTES = pitch_structure_notes()
TRANSIENT_MOTION_NOTES = transient_motion_notes()
FEATURE_NOTES = {
    "cqt": HARMONIC_NOTES,
    "reassigned-stft": TRANSIENT_MOTION_NOTES,
    "chroma": PITCH_STRUCTURE_NOTES,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ui-template", type=Path, required=True)
    return parser.parse_args()


def load_rgb(path: Path) -> Image.Image:
    with Image.open(path) as image:
        return image.convert("RGB")


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(ROBOTO_PATH, size=size)


def package_snapshots(input_root: Path, output_root: Path) -> None:
    for spec in TRANSFORMS:
        source = input_root / "snapshots" / f"{spec.slug}.ppm"
        if not source.exists():
            raise FileNotFoundError(source)
        load_rgb(source).save(
            output_root / f"transform-{spec.slug}.webp",
            format="WEBP",
            quality=92,
            method=6,
        )


def is_black_key(midi_note: int) -> bool:
    return midi_note % 12 in {1, 3, 6, 8, 10}


def render_piano_roll(
    notes: Iterable[Note],
    *,
    width: int,
    height: int,
    progress: float,
) -> Image.Image:
    """Render the same sleek piano-roll style used by the example cards."""
    key_width = max(60, round(width * 0.093))
    min_note, max_note = 28, 76
    row_height = height / (max_note - min_note)
    timeline_width = width - key_width
    image = Image.new("RGB", (width, height), "#080c14")
    draw = ImageDraw.Draw(image)

    for midi_note in range(min_note, max_note):
        top = round((max_note - midi_note - 1) * row_height)
        bottom = round((max_note - midi_note) * row_height)
        key_color = "#202735" if is_black_key(midi_note) else "#d9dce2"
        lane_color = "#0c121e" if is_black_key(midi_note) else "#111926"
        draw.rectangle((0, top, key_width - 1, bottom), fill=key_color)
        draw.rectangle((key_width, top, width, bottom), fill=lane_color)
        draw.line((0, bottom, width, bottom), fill="#263044", width=1)

    for beat in range(17):
        x = key_width + round(timeline_width * beat / 16.0)
        color = "#44506a" if beat % 4 == 0 else "#252f42"
        draw.line((x, 0, x, height), fill=color, width=2 if beat % 4 == 0 else 1)

    playback_position = (progress % 1.0) * 4.0
    for note in notes:
        left = key_width + round(note.start / 4.0 * timeline_width)
        right = key_width + round((note.start + note.duration) / 4.0 * timeline_width)
        top = round((max_note - note.midi_note - 1) * row_height) + 1
        bottom = round((max_note - note.midi_note) * row_height) - 1
        intensity = max(0.0, min(1.0, note.velocity))
        active = note.start <= playback_position <= note.start + note.duration
        color = (
            (
                round(164 + 76 * intensity),
                round(115 + 70 * intensity),
                min(255, round(192 + 62 * intensity)),
            )
            if active
            else (
                round(132 + 78 * intensity),
                round(77 + 56 * intensity),
                round(162 + 70 * intensity),
            )
        )
        draw.rounded_rectangle((left, top, max(right, left + 4), bottom), radius=3, fill=color)
        draw.line((left + 2, top + 1, max(right - 2, left + 2), top + 1), fill="#f2ddff")

    playhead_x = key_width + round((progress % 1.0) * timeline_width)
    draw.rectangle((playhead_x - 2, 0, playhead_x + 2, height), fill="#40e0c2")
    draw.polygon(
        ((playhead_x - 7, 0), (playhead_x + 7, 0), (playhead_x, 10)),
        fill="#d2fff5",
    )
    return image


def patch_centered_text(
    image: Image.Image,
    center: tuple[int, int],
    value: str,
    *,
    width: int,
    height: int,
    text_size: int,
    background: tuple[int, int, int],
    foreground: tuple[int, int, int] = (226, 232, 242),
) -> None:
    draw = ImageDraw.Draw(image)
    x, y = center
    box = (x - width // 2, y - height // 2, x + width // 2, y + height // 2)
    draw.rectangle(box, fill=background)
    selected_font = font(text_size)
    if draw.textlength(value, font=selected_font) > width - 8:
        selected_font = font(max(16, text_size - 3))
    draw.text((x, y), value, font=selected_font, fill=foreground, anchor="mm")


def patch_menu_value(image: Image.Image, control: str, value: str) -> None:
    patch_centered_text(
        image,
        MENU_VALUE_CENTERS[control],
        value,
        width=304,
        height=38,
        text_size=24,
        background=(18, 23, 33),
    )


def patch_slider_value(image: Image.Image, control: str, value: str) -> None:
    patch_centered_text(
        image,
        SLIDER_VALUE_CENTERS[control],
        value,
        width=190,
        height=34,
        text_size=22,
        background=(12, 16, 22),
    )


def format_parameter_value(control: str, value: float) -> str:
    if control in {"fft_size", "bins_per_octave", "bands"}:
        return str(round(value))
    if control == "q_scale":
        return f"{value:.2f} x"
    if control == "gamma":
        return f"{value:.1f} Hz"
    raise ValueError(f"unsupported tutorial parameter: {control}")


def parameter_value(spec: TransformSpec, phase: float) -> float:
    eased = ease_in_out(phase)
    if spec.parameter in MENU_VALUE_CENTERS:
        return spec.parameter_end if eased >= 0.5 else spec.parameter_start
    return spec.parameter_start + (spec.parameter_end - spec.parameter_start) * eased


def slider_normalized(control: str, value: float) -> float:
    minimum, maximum = SLIDER_RANGES[control]
    return max(0.0, min(1.0, (value - minimum) / (maximum - minimum)))


def slider_knob_position(control: str, value: float) -> tuple[float, float]:
    left, right, y = SLIDER_TRACKS[control]
    return (left + (right - left) * slider_normalized(control, value), float(y))


def patch_slider_state(image: Image.Image, control: str, value: float) -> None:
    left, right, y = SLIDER_TRACKS[control]
    knob_x = round(slider_knob_position(control, value)[0])
    draw = ImageDraw.Draw(image)

    # Replace the captured track and knob as a single clipped control. This
    # prevents the original knob from ghosting underneath an animated sweep.
    draw.rectangle((left - 13, y - 14, right + 13, y + 14), fill=(12, 16, 22))
    draw.rounded_rectangle(
        (left, y - 8, right, y + 8),
        radius=3,
        fill=(30, 38, 52),
        outline=(43, 54, 74),
        width=2,
    )
    draw.rounded_rectangle(
        (left + 1, y - 7, max(left + 2, knob_x), y + 7),
        radius=2,
        fill=(103, 84, 238),
    )
    draw.ellipse(
        (knob_x - 11, y - 11, knob_x + 11, y + 11),
        fill=(227, 233, 246),
        outline=(76, 89, 111),
        width=2,
    )
    patch_slider_value(image, control, format_parameter_value(control, value))


def parameter_cursor_position(spec: TransformSpec, phase: float) -> tuple[float, float]:
    if spec.parameter in MENU_VALUE_CENTERS:
        return CURSOR_POSITIONS[spec.parameter]
    start_x, start_y = slider_knob_position(spec.parameter, spec.parameter_start)
    direction = -1.0 if spec.parameter_end > spec.parameter_start else 1.0
    return (
        start_x,
        start_y + direction * SLIDER_DRAG_DISTANCE * ease_in_out(phase),
    )


def patch_header_title(image: Image.Image, value: str) -> None:
    draw = ImageDraw.Draw(image)
    draw.rectangle(HEADER_TITLE_BOX, fill=(14, 18, 26))
    draw.text(
        (74, (HEADER_TITLE_BOX[1] + HEADER_TITLE_BOX[3]) // 2),
        value,
        font=font(26),
        fill=(226, 232, 242),
        anchor="lm",
    )


def paste_clipped_plot(image: Image.Image, plot: Image.Image) -> None:
    width = PLOT_RECT[2] - PLOT_RECT[0]
    height = PLOT_RECT[3] - PLOT_RECT[1]
    rendered = plot.resize((width, height), Image.Resampling.BICUBIC)
    image.paste(rendered, PLOT_RECT[:2])

    draw = ImageDraw.Draw(image)
    draw.rectangle(
        (PLOT_RECT[0], PLOT_RECT[1], PLOT_RECT[2] - 1, PLOT_RECT[3] - 1),
        outline=(31, 40, 54),
        width=2,
    )
    draw.line(
        (PLOT_RECT[2] - 1, PLOT_RECT[1], PLOT_RECT[2] - 1, PLOT_RECT[3] - 1),
        fill=(61, 206, 177),
        width=2,
    )


def draw_cursor(
    image: Image.Image,
    position: tuple[float, float],
    *,
    clicking: bool,
) -> None:
    x, y = (round(position[0]), round(position[1]))
    draw = ImageDraw.Draw(image, "RGBA")
    if clicking:
        draw.ellipse((x - 20, y - 20, x + 20, y + 20), fill=(61, 206, 177, 58))
        draw.ellipse((x - 12, y - 12, x + 12, y + 12), outline=(116, 244, 218, 190), width=3)
    outline = (
        (x, y),
        (x, y + 34),
        (x + 9, y + 25),
        (x + 17, y + 42),
        (x + 25, y + 38),
        (x + 17, y + 21),
        (x + 31, y + 20),
    )
    fill = (
        (x + 3, y + 4),
        (x + 3, y + 29),
        (x + 10, y + 22),
        (x + 18, y + 37),
        (x + 21, y + 36),
        (x + 14, y + 19),
        (x + 26, y + 18),
    )
    draw.polygon(outline, fill=(5, 8, 13, 235))
    draw.polygon(fill, fill=(245, 248, 252, 255))


def native_plugin_frame(
    template: Image.Image,
    plot: Image.Image,
    spec: TransformSpec,
    *,
    output_width: int,
    display_scale: str = "Log",
    parameter_phase: float = 1.0,
    cursor_position: tuple[float, float] | None = None,
    clicking: bool = False,
) -> Image.Image:
    image = template.copy()
    paste_clipped_plot(image, plot)
    patch_menu_value(image, "transform", spec.menu_label)
    patch_menu_value(image, "fft_size", "8192")
    patch_menu_value(image, "display_scale", display_scale)
    patch_header_title(image, spec.header_label)

    value = parameter_value(spec, parameter_phase)
    if spec.parameter in MENU_VALUE_CENTERS:
        patch_menu_value(image, spec.parameter, format_parameter_value(spec.parameter, value))
    else:
        patch_slider_state(image, spec.parameter, value)

    if cursor_position is not None:
        draw_cursor(image, cursor_position, clicking=clicking)

    output_height = round(image.height * output_width / image.width)
    return image.resize((output_width, output_height), Image.Resampling.LANCZOS)


def quantize_frames(frames: Sequence[Image.Image]) -> list[Image.Image]:
    if not frames:
        raise ValueError("at least one GIF frame is required")
    palette = frames[0].quantize(colors=192, method=Image.Quantize.FASTOCTREE)
    return [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]


def write_gif(frames: Sequence[Image.Image], destination: Path) -> None:
    prepared = quantize_frames(frames)
    prepared[0].save(
        destination,
        save_all=True,
        append_images=prepared[1:],
        duration=FEATURE_FRAME_DURATION_MS,
        loop=0,
        disposal=2,
        optimize=False,
    )


def write_streamed_animation(frames: Iterable[Image.Image], destination: Path) -> None:
    """Encode long tours without retaining every full-resolution frame in RAM."""
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise RuntimeError("ffmpeg is required to package the long tutorial GIFs")
    with tempfile.TemporaryDirectory(prefix="sinesoda-site-gif-") as temporary:
        root = Path(temporary)
        frame_count = 0
        for frame_count, frame in enumerate(frames, start=1):
            frame.save(root / f"frame-{frame_count - 1:04d}.png", compress_level=2)
        if frame_count == 0:
            raise ValueError("at least one GIF frame is required")
        common = (
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-framerate",
            str(TUTORIAL_FPS),
            "-i",
            str(root / "frame-%04d.png"),
        )
        if destination.suffix == ".gif":
            encoding = (
                "-vf",
                "split[s0][s1];[s0]palettegen=max_colors=192:stats_mode=diff[p];"
                "[s1][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle",
                "-loop",
                "0",
            )
        elif destination.suffix == ".mp4":
            encoding = (
                "-c:v",
                "libx264",
                "-crf",
                "24",
                "-pix_fmt",
                "yuv420p",
                "-movflags",
                "+faststart",
            )
        else:
            raise ValueError(f"unsupported animation format: {destination.suffix}")
        command = (*common, *encoding, str(destination.resolve()))
        completed = subprocess.run(command, capture_output=True, check=False, text=True)
        if completed.returncode != 0:
            raise RuntimeError(f"ffmpeg GIF packaging failed: {completed.stderr.strip()}")


def load_animation_frames(frame_root: Path) -> list[Image.Image]:
    paths = sorted(frame_root.glob("frame-*.ppm"))
    if not paths:
        raise FileNotFoundError(f"no animation frames in {frame_root}")
    return [load_rgb(path) for path in paths]


def scroll_plot(plot: Image.Image, progress: float) -> Image.Image:
    """Advance one cyclic analyzer history in lockstep with the piano roll."""
    offset = round((progress % 1.0) * plot.width)
    if offset == 0:
        return plot
    shifted = Image.new("RGB", plot.size)
    shifted.paste(plot, (-offset, 0))
    shifted.paste(plot, (plot.width - offset, 0))
    return shifted


def frequency_domain_view(plot: Image.Image, position: float) -> Image.Image:
    """Crop a magnified frequency domain at a normalized low/high position."""
    clamped = max(-1.0, min(1.0, position))
    expanded_height = round(plot.height * 1.7)
    expanded = plot.resize((plot.width, expanded_height), Image.Resampling.BICUBIC)
    maximum_offset = expanded_height - plot.height
    crop_top = round((1.0 - clamped) * 0.5 * maximum_offset)
    return expanded.crop((0, crop_top, plot.width, crop_top + plot.height))


def compose_stacked_frame(
    piano: Image.Image,
    plugin: Image.Image,
    *,
    width: int,
) -> Image.Image:
    canvas = Image.new("RGB", (width, piano.height + plugin.height), (5, 8, 13))
    canvas.paste(piano, (0, 0))
    canvas.paste(plugin, (0, piano.height))
    return canvas


def package_feature_animations(
    input_root: Path,
    output_root: Path,
    template: Image.Image,
) -> None:
    exports = {
        "cqt": "harmonic-resolution.gif",
        "reassigned-stft": "transient-localization.gif",
        "chroma": "pitch-class-motion.gif",
    }
    for slug, filename in exports.items():
        spec = TRANSFORM_BY_SLUG[slug]
        plots = load_animation_frames(input_root / "animations" / slug)
        if len(plots) != ANALYSIS_FRAME_COUNT:
            raise ValueError(
                f"expected {ANALYSIS_FRAME_COUNT} animation frames for {slug}, got {len(plots)}"
            )
        frames = []
        for frame_index, plot in enumerate(plots):
            progress = frame_index / len(plots)
            piano = render_piano_roll(
                FEATURE_NOTES[slug],
                width=FEATURE_OUTPUT_WIDTH,
                height=FEATURE_PIANO_HEIGHT,
                progress=progress,
            )
            plugin = native_plugin_frame(
                template,
                plot,
                spec,
                output_width=FEATURE_OUTPUT_WIDTH,
            )
            frames.append(compose_stacked_frame(piano, plugin, width=FEATURE_OUTPUT_WIDTH))
        write_gif(frames, output_root / filename)


def ease_in_out(value: float) -> float:
    clamped = max(0.0, min(1.0, value))
    return clamped * clamped * (3.0 - 2.0 * clamped)


def frame_phase(frame: int, start: int, end: int) -> float:
    """Return a clamped phase whose inclusive endpoint is ``end``."""
    return max(0.0, min(1.0, (frame - start) / max(end - start, 1)))


def interpolate_point(
    start: tuple[float, float],
    end: tuple[float, float],
    phase: float,
) -> tuple[float, float]:
    eased = ease_in_out(phase)
    return (
        start[0] + (end[0] - start[0]) * eased,
        start[1] + (end[1] - start[1]) * eased,
    )


def compose_tutorial_frame(piano: Image.Image, plugin: Image.Image) -> Image.Image:
    return compose_stacked_frame(piano, plugin, width=TUTORIAL_OUTPUT_WIDTH)


def timeline_progress(frame_index: int) -> float:
    return (frame_index % PLAYHEAD_PERIOD_FRAMES) / PLAYHEAD_PERIOD_FRAMES


def analyzer_progress(frame_index: int) -> float:
    return timeline_progress(frame_index)


def piano_for_timeline(frame_index: int) -> Image.Image:
    return render_piano_roll(
        HARMONIC_NOTES,
        width=TUTORIAL_OUTPUT_WIDTH,
        height=TUTORIAL_PIANO_HEIGHT,
        progress=timeline_progress(frame_index),
    )


def parameter_plot(
    endpoints: tuple[Image.Image, Image.Image],
    spec: TransformSpec,
    parameter_phase: float,
    scroll_progress: float,
) -> Image.Image:
    low_plot, high_plot = endpoints
    value = parameter_value(spec, parameter_phase)
    low_value = min(spec.parameter_start, spec.parameter_end)
    high_value = max(spec.parameter_start, spec.parameter_end)
    mix = (value - low_value) / max(high_value - low_value, 1.0e-9)
    if mix <= 0.0:
        return scroll_plot(low_plot, scroll_progress)
    if mix >= 1.0:
        return scroll_plot(high_plot, scroll_progress)
    return Image.blend(
        scroll_plot(low_plot, scroll_progress),
        scroll_plot(high_plot, scroll_progress),
        mix,
    )


def transform_tutorial_frames(
    plots: dict[str, tuple[Image.Image, Image.Image]],
    template: Image.Image,
) -> Iterator[Image.Image]:
    global_frame = 0
    for transform_index, spec in enumerate(TRANSFORMS):
        previous = TRANSFORMS[max(0, transform_index - 1)]
        previous_cursor = parameter_cursor_position(previous, 1.0)
        transform_cursor = CURSOR_POSITIONS["transform"]
        parameter_cursor = parameter_cursor_position(spec, 0.0)
        for local_frame in range(TRANSFORM_STEP_FRAMES):
            progress = analyzer_progress(global_frame)
            if local_frame < TRANSFORM_MOVE_END:
                cursor = interpolate_point(
                    previous_cursor,
                    transform_cursor,
                    frame_phase(local_frame, 0, TRANSFORM_MOVE_END - 1),
                )
            elif local_frame < PARAMETER_MOVE_START:
                cursor = transform_cursor
            elif local_frame < PARAMETER_MOVE_END:
                cursor = interpolate_point(
                    transform_cursor,
                    parameter_cursor,
                    frame_phase(
                        local_frame,
                        PARAMETER_MOVE_START,
                        PARAMETER_MOVE_END - 1,
                    ),
                )
            elif local_frame < PARAMETER_DRAG_START:
                cursor = parameter_cursor
            else:
                cursor = parameter_cursor_position(
                    spec,
                    frame_phase(local_frame, PARAMETER_DRAG_START, PARAMETER_DRAG_END),
                )

            transform_changed = local_frame >= TRANSFORM_CHANGE_FRAME or transform_index == 0
            if spec.parameter in SLIDER_TRACKS:
                parameter_phase = frame_phase(
                    local_frame,
                    PARAMETER_DRAG_START,
                    PARAMETER_DRAG_END,
                )
            else:
                parameter_phase = 1.0 if local_frame >= PARAMETER_MENU_CHANGE else 0.0
            active_spec = spec if transform_changed else previous
            active_parameter_phase = parameter_phase if active_spec == spec else 1.0
            if transform_changed and transform_index > 0 and local_frame < TRANSFORM_BLEND_END:
                blend = ease_in_out(
                    frame_phase(
                        local_frame,
                        TRANSFORM_CHANGE_FRAME,
                        TRANSFORM_BLEND_END - 1,
                    )
                )
                plot = Image.blend(
                    parameter_plot(plots[previous.slug], previous, 1.0, progress),
                    parameter_plot(plots[spec.slug], spec, 0.0, progress),
                    blend,
                )
            else:
                plot = parameter_plot(
                    plots[active_spec.slug], active_spec, active_parameter_phase, progress
                )

            dragging_slider = (
                spec.parameter in SLIDER_TRACKS
                and PARAMETER_DRAG_START <= local_frame <= PARAMETER_DRAG_END
            )
            clicking_menu = (
                spec.parameter in MENU_VALUE_CENTERS
                and PARAMETER_DRAG_START <= local_frame <= PARAMETER_MENU_CHANGE
            )
            clicking_transform = abs(local_frame - TRANSFORM_CHANGE_FRAME) <= 1
            clicking = clicking_transform or dragging_slider or clicking_menu
            plugin = native_plugin_frame(
                template,
                plot,
                active_spec,
                output_width=TUTORIAL_OUTPUT_WIDTH,
                parameter_phase=active_parameter_phase,
                cursor_position=cursor,
                clicking=clicking,
            )
            yield compose_tutorial_frame(piano_for_timeline(global_frame), plugin)
            global_frame += 1


def package_transform_tutorial(
    input_root: Path,
    output_root: Path,
    template: Image.Image,
) -> None:
    plots = {
        spec.slug: (
            load_rgb(input_root / "parameter-sweeps" / f"{spec.slug}-low.ppm"),
            load_rgb(input_root / "parameter-sweeps" / f"{spec.slug}-high.ppm"),
        )
        for spec in TRANSFORMS
    }
    write_streamed_animation(
        transform_tutorial_frames(plots, template),
        output_root / "tutorial-transform-tour.mp4",
    )


def scale_domain_endpoints(scale: str) -> tuple[float, float]:
    if scale == "Native":
        return (1.0, -1.0)
    if scale == "Linear":
        return (-1.0, 1.0)
    return (0.0, 0.0)


def scale_plot_frame(
    plot: Image.Image,
    scale: str,
    timeline_position: float,
    domain_position: float,
) -> Image.Image:
    viewed = frequency_domain_view(plot, domain_position) if scale != "Log" else plot
    return scroll_plot(viewed, timeline_position)


def scale_tutorial_frames(
    plots: dict[str, Image.Image],
    template: Image.Image,
) -> Iterator[Image.Image]:
    spec = TRANSFORM_BY_SLUG["stft"]
    scales = tuple(plots)
    global_frame = 0
    previous_cursor = SCALE_CURSOR_IDLE
    for scale_index, scale in enumerate(scales):
        previous = scales[max(0, scale_index - 1)]
        target = CURSOR_POSITIONS["display_scale"]
        scrollable = scale != "Log"
        first_domain, second_domain = scale_domain_endpoints(scale)
        first_cursor = SCALE_DOMAIN_CURSOR_UP if first_domain > 0.0 else SCALE_DOMAIN_CURSOR_DOWN
        second_cursor = SCALE_DOMAIN_CURSOR_UP if second_domain > 0.0 else SCALE_DOMAIN_CURSOR_DOWN
        for local_frame in range(SCALE_STEP_FRAMES):
            progress = analyzer_progress(global_frame)
            if local_frame < SCALE_MOVE_END:
                cursor = interpolate_point(
                    previous_cursor,
                    target,
                    frame_phase(local_frame, 0, SCALE_MOVE_END - 1),
                )
            elif scrollable and local_frame < SCALE_BLEND_END:
                cursor = target
            elif scrollable and local_frame < SCALE_DOMAIN_MOVE_END:
                cursor = interpolate_point(
                    target,
                    SCALE_DOMAIN_CURSOR,
                    frame_phase(
                        local_frame,
                        SCALE_BLEND_END,
                        SCALE_DOMAIN_MOVE_END - 1,
                    ),
                )
            elif scrollable and local_frame < SCALE_DOMAIN_FIRST_DRAG_END:
                cursor = interpolate_point(
                    SCALE_DOMAIN_CURSOR,
                    first_cursor,
                    frame_phase(
                        local_frame,
                        SCALE_DOMAIN_MOVE_END,
                        SCALE_DOMAIN_FIRST_DRAG_END - 1,
                    ),
                )
            elif scrollable and local_frame < SCALE_DOMAIN_SECOND_DRAG_END:
                cursor = interpolate_point(
                    first_cursor,
                    second_cursor,
                    frame_phase(
                        local_frame,
                        SCALE_DOMAIN_FIRST_DRAG_END,
                        SCALE_DOMAIN_SECOND_DRAG_END - 1,
                    ),
                )
            elif scrollable:
                cursor = second_cursor
            elif local_frame < SCALE_RETURN_START:
                cursor = target
            elif local_frame < SCALE_RETURN_END:
                cursor = interpolate_point(
                    target,
                    SCALE_CURSOR_IDLE,
                    frame_phase(
                        local_frame,
                        SCALE_RETURN_START,
                        SCALE_RETURN_END - 1,
                    ),
                )
            else:
                cursor = SCALE_CURSOR_IDLE

            if scrollable and local_frame >= SCALE_DOMAIN_MOVE_END:
                if local_frame < SCALE_DOMAIN_FIRST_DRAG_END:
                    domain_phase = frame_phase(
                        local_frame,
                        SCALE_DOMAIN_MOVE_END,
                        SCALE_DOMAIN_FIRST_DRAG_END - 1,
                    )
                    domain_position = first_domain * ease_in_out(domain_phase)
                else:
                    domain_phase = frame_phase(
                        local_frame,
                        SCALE_DOMAIN_FIRST_DRAG_END,
                        SCALE_DOMAIN_SECOND_DRAG_END - 1,
                    )
                    domain_position = first_domain + (
                        second_domain - first_domain
                    ) * ease_in_out(domain_phase)
            else:
                domain_position = 0.0

            scale_changed = local_frame >= SCALE_CHANGE_FRAME or scale_index == 0
            active_scale = scale if scale_changed else previous
            if scale_changed and scale_index > 0 and local_frame < SCALE_BLEND_END:
                blend = ease_in_out(
                    frame_phase(
                        local_frame,
                        SCALE_CHANGE_FRAME,
                        SCALE_BLEND_END - 1,
                    )
                )
                plot = Image.blend(
                    scale_plot_frame(
                        plots[previous],
                        previous,
                        progress,
                        scale_domain_endpoints(previous)[1],
                    ),
                    scale_plot_frame(plots[scale], scale, progress, 0.0),
                    blend,
                )
            else:
                active_domain_position = (
                    domain_position
                    if active_scale == scale
                    else scale_domain_endpoints(previous)[1]
                )
                plot = scale_plot_frame(
                    plots[active_scale],
                    active_scale,
                    progress,
                    active_domain_position,
                )

            dragging_domain = (
                scrollable
                and SCALE_DOMAIN_MOVE_END
                <= local_frame
                < SCALE_DOMAIN_SECOND_DRAG_END
            )

            plugin = native_plugin_frame(
                template,
                plot,
                spec,
                output_width=TUTORIAL_OUTPUT_WIDTH,
                display_scale=active_scale,
                cursor_position=cursor,
                clicking=abs(local_frame - SCALE_CHANGE_FRAME) <= 1 or dragging_domain,
            )
            yield compose_tutorial_frame(piano_for_timeline(global_frame), plugin)
            global_frame += 1
        previous_cursor = second_cursor if scrollable else SCALE_CURSOR_IDLE


def package_scale_tutorial(
    input_root: Path,
    output_root: Path,
    template: Image.Image,
) -> None:
    plots = {
        "Native": load_rgb(input_root / "scales" / "linear.ppm"),
        "Log": load_rgb(input_root / "scales" / "log.ppm"),
        "Linear": load_rgb(input_root / "scales" / "linear.ppm"),
    }
    write_streamed_animation(
        scale_tutorial_frames(plots, template),
        output_root / "tutorial-scale-tour.gif",
    )


def main() -> None:
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    template = load_rgb(args.ui_template)
    if template.size != (2228, 1470):
        raise ValueError(f"unexpected UI template dimensions: {template.size}")

    package_snapshots(args.input, args.output)
    package_feature_animations(args.input, args.output, template)
    package_transform_tutorial(args.input, args.output, template)
    package_scale_tutorial(args.input, args.output, template)


if __name__ == "__main__":
    main()
