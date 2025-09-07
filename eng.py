import sounddevice as sd
import numpy as np
import librosa
import queue
import threading
import time
import os
import sys # Added for sys.stdout.isatty()
# pip install colorama (Recommended for proper color display on Windows)
try:
    import colorama
    colorama.init() # Enables ANSI color codes for Windows
except ImportError:
    pass

# ==================
# Color Codes
# ==================
class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'

    BRIGHT_BLACK = '\033[90m'
    BRIGHT_RED = '\033[91m'
    BRIGHT_GREEN = '\033[92m'
    BRIGHT_YELLOW = '\033[93m'
    BRIGHT_BLUE = '\033[94m'
    BRIGHT_MAGENTA = '\033[95m'
    BRIGHT_CYAN = '\033[96m'
    BRIGHT_WHITE = '\033[97m'

    BG_BLACK = '\033[40m'
    BG_RED = '\033[41m'
    BG_GREEN = '\033[42m'
    BG_YELLOW = '\033[43m'
    BG_BLUE = '\033[44m'
    BG_MAGENTA = '\033[45m'
    BG_CYAN = '\033[46m'
    BG_WHITE = '\033[47m'

    @staticmethod
    def get_color(color_code):
        """Checks if the terminal supports colors."""
        if sys.stdout.isatty():
            return color_code
        return ""

# ==================
# General Settings
# ==================
SAMPLE_RATE = 22050
BLOCK_SIZE = 1024
CHANNELS = 1
DTYPE = 'float32'
RECORDING_FILENAME = "recorded_processed_audio.wav"

# ====================================
# 1. RECORD → PROCESS → PLAY → SAVE Mode
# ====================================

def record_process_play_save_mode():
    s = input("How many seconds would you like to record for: ")
    DURATION = int(s)  # seconds

    print(f"{Colors.get_color(Colors.BRIGHT_CYAN)}[RECORDING] Start speaking ({{}} seconds)...{Colors.RESET}".format(DURATION))
    recording = sd.rec(int(DURATION * SAMPLE_RATE), samplerate=SAMPLE_RATE, channels=CHANNELS, dtype=DTYPE)
    sd.wait()
    print(f"{Colors.get_color(Colors.BRIGHT_CYAN)}[RECORDING] Finished.{Colors.RESET}")

    print(f"{Colors.get_color(Colors.BRIGHT_YELLOW)}[PROCESSING] Processing audio...{Colors.RESET}")
    # Apply pitch shift (e.g., -4 semitones)
    processed_1d = librosa.effects.pitch_shift(recording.flatten(), sr=SAMPLE_RATE, n_steps=-4)
    # Add some light noise for an "anonymizing" effect
    noise = np.random.normal(0, 0.005, processed_1d.shape).astype(DTYPE)
    processed_1d = processed_1d + noise
    # Clip values to ensure they are within the valid audio range [-1.0, 1.0]
    processed_1d = np.clip(processed_1d, -1.0, 1.0)

    print(f"{Colors.get_color(Colors.BRIGHT_MAGENTA)}[PLAYBACK] Playing processed audio...{Colors.RESET}")
    sd.play(processed_1d, SAMPLE_RATE)
    sd.wait()
    print(f"{Colors.get_color(Colors.BRIGHT_MAGENTA)}[PLAYBACK] Finished.{Colors.RESET}")

    try:
        import soundfile as sf
        sf.write(RECORDING_FILENAME, processed_1d, SAMPLE_RATE)
        print(f"{Colors.get_color(Colors.BRIGHT_GREEN)}[SAVE] Processed audio saved to '{RECORDING_FILENAME}'.{Colors.RESET}")
    except ImportError:
        print(f"{Colors.get_color(Colors.RED)}[ERROR] 'soundfile' library not installed. Please install with 'pip install soundfile'. Cannot save audio.{Colors.RESET}")
    except Exception as e:
        print(f"{Colors.get_color(Colors.RED)}[ERROR] An issue occurred while saving audio: {e}{Colors.RESET}")

# =====================
# 2. REAL-TIME Mode
# =====================

input_q = queue.Queue()
output_q = queue.Queue()

def audio_input_callback(indata, frames, time_info, status):
    """Callback function for the audio input stream."""
    if status:
        print(f"{Colors.get_color(Colors.YELLOW)}[Warning-Input]{Colors.RESET} {status}")
    input_q.put(indata.copy())

def audio_output_callback(outdata, frames, time_info, status):
    """Callback function for the audio output stream."""
    if status:
        print(f"{Colors.get_color(Colors.YELLOW)}[Warning-Output]{Colors.RESET} {status}")
    try:
        # Get processed data from the output queue
        data = output_q.get_nowait()
        if len(data) >= len(outdata):
            outdata[:] = data[:len(outdata)]
            if len(data) > len(outdata):
                # If there's leftover data, put it back in the queue
                output_q.put_nowait(data[len(outdata):])
        else:
            # If not enough data, pad with zeros
            outdata.fill(0)
    except queue.Empty:
        # If queue is empty, output silence
        outdata.fill(0)

def realtime_processor():
    """Processes audio chunks in real-time."""
    print(f"{Colors.get_color(Colors.BRIGHT_BLUE)}[REAL-TIME] Audio Processor Started.{Colors.RESET}")
    while True:
        # Get raw audio from the input queue
        input_data = input_q.get()
        # Apply pitch shift
        processed_1d = librosa.effects.pitch_shift(input_data.flatten(), sr=SAMPLE_RATE, n_steps=-4)
        # Add some noise
        noise = np.random.normal(0, 0.003, processed_1d.shape).astype(DTYPE)
        processed_1d = processed_1d + noise
        # Clip values
        processed_1d = np.clip(processed_1d, -1.0, 1.0)
        # Reshape to 2D for the output stream
        processed_2d = processed_1d.reshape(-1, CHANNELS)
        # Put processed data into the output queue
        output_q.put(processed_2d)

def realtime_mode():
    """Initializes and runs the real-time audio processing."""
    print(f"{Colors.get_color(Colors.BRIGHT_GREEN)}{Colors.BOLD}*** REAL-TIME VOICE CHANGER MODE ***{Colors.RESET}")
    print(f"{Colors.get_color(Colors.BRIGHT_WHITE)}Microphone → Anonymized Voice (Press {Colors.RED}Ctrl+C{Colors.BRIGHT_WHITE} to exit)")

    processor_thread = threading.Thread(target=realtime_processor, daemon=True)
    processor_thread.start()

    # Open input and output audio streams
    with sd.InputStream(callback=audio_input_callback,
                        samplerate=SAMPLE_RATE,
                        channels=CHANNELS,
                        dtype=DTYPE,
                        blocksize=BLOCK_SIZE) as input_stream, \
         sd.OutputStream(callback=audio_output_callback,
                         samplerate=SAMPLE_RATE,
                         channels=CHANNELS,
                         dtype=DTYPE,
                         blocksize=BLOCK_SIZE) as output_stream:
        print(f"{Colors.get_color(Colors.BRIGHT_GREEN)}Real-time stream started... Speak and hear the processed voice.{Colors.RESET}")
        # Keep the main thread alive while streams are active
        while input_stream.is_active and output_stream.is_active:
            time.sleep(0.1)
        print(f"{Colors.get_color(Colors.RED)}Stream stopped.{Colors.RESET}")

# ==================
# Main Menu
# ==================

def display_menu():
    c = Colors # Shorthand for shorter code
    print(f"\n{c.get_color(c.BRIGHT_YELLOW)}{c.BOLD}{'='*40}{c.RESET}")
    print(f"{c.get_color(c.BRIGHT_CYAN)}{c.BOLD}{' ':10}AUDIO PROCESSING APP{' ':10}{c.RESET}")
    print(f"{c.get_color(c.BRIGHT_YELLOW)}{c.BOLD}{'='*40}{c.RESET}")

    print(f"{c.get_color(c.BRIGHT_WHITE)} {c.BOLD}Options:{c.RESET}")
    print(f" {c.get_color(c.GREEN)}{c.BOLD}1.{c.RESET} {c.BRIGHT_WHITE}Record, Process, Play, and Save Mixed Audio {c.RESET}{c.get_color(c.BRIGHT_BLACK)}(User-defined recording duration){c.RESET}")
    print(f" {c.get_color(c.BLUE)}{c.BOLD}2.{c.RESET} {c.BRIGHT_WHITE}Real-time Voice Changer {c.RESET}{c.get_color(c.BRIGHT_BLACK)}(Microphone → Headphones){c.RESET}")
    print(f" {c.get_color(c.RED)}{c.BOLD}3.{c.RESET} {c.BRIGHT_WHITE}Exit{c.RESET}")
    print(f"{c.get_color(c.BRIGHT_YELLOW)}{c.BOLD}{'='*40}{c.RESET}")

def main():
    while True:
        display_menu()
        choice = input(f"{Colors.get_color(Colors.BRIGHT_WHITE)}Please enter an option {Colors.get_color(Colors.CYAN)}(1-3){Colors.BRIGHT_WHITE}: {Colors.RESET}")

        if choice == '1':
            print(f"\n{Colors.get_color(Colors.MAGENTA)}{Colors.BOLD}>>> Selection: Record & Process Mode <<<{Colors.RESET}\n")
            record_process_play_save_mode()
            print(f"\n{Colors.get_color(Colors.BRIGHT_YELLOW)}{'='*50}{Colors.RESET}\n")
        elif choice == '2':
            print(f"\n{Colors.get_color(Colors.MAGENTA)}{Colors.BOLD}>>> Selection: Real-time Mode <<<{Colors.RESET}\n")
            try:
                realtime_mode()
            except KeyboardInterrupt:
                print(f"\n{Colors.get_color(Colors.RED)}Exiting real-time mode.{Colors.RESET}")
            except Exception as e:
                print(f"{Colors.get_color(Colors.RED)}[ERROR] An error occurred: {e}{Colors.RESET}")
            print(f"\n{Colors.get_color(Colors.BRIGHT_YELLOW)}{'='*50}{Colors.RESET}\n")
        elif choice == '3':
            print(f"{Colors.get_color(Colors.BRIGHT_RED)}Exiting the program. Goodbye!{Colors.RESET}")
            break
        else:
            print(f"{Colors.get_color(Colors.RED)}Invalid option. Please enter {Colors.CYAN}1, 2{Colors.RED} or {Colors.RED}3{Colors.RESET}{Colors.RED}.{Colors.RESET}")
            time.sleep(1)

if __name__ == "__main__":
    main()