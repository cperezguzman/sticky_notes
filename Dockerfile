# Multi-stage image: build SDL3 + app in builder; run CLI or GUI from slim runtime.
# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
	build-essential \
	ca-certificates \
	cmake \
	curl \
	make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY Makefile scripts/ src/ sandbox/ assets/ ./

RUN ./scripts/build-sdl3.sh \
    && make sticky_notes textbox \
    && ln -sf textbox_sandbox sticky-notes

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Shared libraries for vendored libSDL3.so (X11 + Wayland paths).
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
	libasound2t64 \
	libdecor-0-0 \
	libdbus-1-3 \
	libdrm2 \
	libegl1 \
	libgbm1 \
	libgl1 \
	libglib2.0-0t64 \
	libpulse0 \
	libudev1 \
	libwayland-client0 \
	libwayland-cursor0 \
	libwayland-egl1 \
	libx11-6 \
	libxcursor1 \
	libxext6 \
	libxi6 \
	libxkbcommon0 \
	libxrandr2 \
	libxss1 \
	libxxf86vm1 \
	ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/sticky_notes /app/textbox_sandbox /app/
COPY --from=builder /app/third_party/sdl3-install /app/third_party/sdl3-install
COPY scripts/run-sticky-notes-gui.sh scripts/

RUN ln -sf textbox_sandbox sticky-notes \
    && chmod +x scripts/run-sticky-notes-gui.sh sticky_notes textbox_sandbox

ENV SDL_VIDEODRIVER=x11
ENV SDL_APP_ID=sticky-notes
ENV SDL_APP_NAME=Sticky Notes

# Default: GUI (override in compose for CLI).
CMD ["./scripts/run-sticky-notes-gui.sh"]
