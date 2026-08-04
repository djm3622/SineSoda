.PHONY: bootstrap configure build benchmark qa-macos site-visuals test format format-check check sanitize clean

SITE_VISUALS_PYTHON ?= python3

bootstrap:
	cmake --preset debug

configure:
	cmake --preset debug

build:
	cmake --build --preset debug

test:
	cmake --preset core-debug
	cmake --build --preset core-debug
	ctest --preset core-debug

benchmark:
	cmake --preset core-release
	cmake --build --preset core-release --target SineSodaBenchmark
	./build/core-release/SineSodaBenchmark

qa-macos:
	cmake --preset core-release
	cmake --build --preset core-release --target SineSodaExhaustiveQA
	./build/core-release/SineSodaExhaustiveQA --output artifacts/qa/macos-exhaustive

site-visuals:
	cmake --preset core-release
	cmake --build --preset core-release --target SineSodaSiteVisuals
	./build/core-release/SineSodaSiteVisuals --output artifacts/qa/site-visual-export
	$(SITE_VISUALS_PYTHON) tools/package_site_visuals.py --input artifacts/qa/site-visual-export --output site/assets --ui-template artifacts/qa/site-captures/sinesoda-stft-8192-metronome.png

format:
	cmake --build --preset core-debug --target format

format-check:
	cmake --build --preset core-debug --target format-check

sanitize:
	cmake --preset sanitize
	cmake --build --preset sanitize
	ctest --preset sanitize

check: format-check test

clean:
	cmake -E rm -rf build
