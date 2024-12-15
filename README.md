# WaveCraft

## Introduction

WaveCraft is a C language project designed to generate musical waveforms. It can produce audio files in WAV or MP3 format based on specified note sequences and supports real-time playback using the ALSA library.

## Installation

### Prerequisites

Ensure you have the following libraries installed:

- **libsndfile**: For handling audio files.
- **lame**: For MP3 encoding.
- **alsa-lib**: For audio playback (Linux).
- **Math Library**: Standard C math library.

On Debian/Ubuntu, you can install these dependencies with the following commands:

```sh
sudo apt-get update
sudo apt-get install libsndfile1-dev libmp3lame-dev libasound2-dev
```

### Building

Use the Makefile to build the project:

```sh
make
```

This will compile the source code and generate an executable named `wavecraft` in the `bin/` directory.

## Usage

### Running the Program

To run WaveCraft, use the following command:

```sh
./bin/wavecraft
```

### Command-Line Arguments

Try run `./bin/wavecraft --help`

## Contributing

Contributions are welcome! Please follow these steps:

1. **Fork** this repository.
2. Create a new branch (`git checkout -b feature/new-feature`).
3. Commit your changes (`git commit -m 'Add some feature'`).
4. Push to the branch (`git push origin feature/new-feature`).
5. Submit a Pull Request.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgments

Thank you to everyone who has contributed to and supported this project!
