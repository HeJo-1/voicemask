import sounddevice as sd
import numpy as np
import librosa
import queue
import threading
import time
import os
import sys 
try:
    import colorama
    colorama.init() 
except ImportError:
    pass

# ==================
# Renk Kodları
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
        """Terminalin renkleri destekleyip desteklemediğini kontrol eder."""
        if sys.stdout.isatty():
            return color_code
        return ""

# ==================
# Genel Ayarlar
# ==================
SAMPLE_RATE = 22050
BLOCK_SIZE = 1024
CHANNELS = 1
DTYPE = 'float32'
RECORDING_FILENAME = "kaydedilen_karisik_ses.wav"

# ==================
# 1. KAYDET → İŞLE → DİNLE → KAYDET Modu
# ==================

def record_process_play_save_mode():
    s = input("Kaç saniye kayıt yapılsın : ")
    DURATION = s  # saniye

    print(f"{Colors.get_color(Colors.BRIGHT_CYAN)}[KAYIT] Konuşmaya başlayın ({{}} saniye)...{Colors.RESET}".format(DURATION))
    recording = sd.rec(int(DURATION * SAMPLE_RATE), samplerate=SAMPLE_RATE, channels=CHANNELS, dtype=DTYPE)
    sd.wait()
    print(f"{Colors.get_color(Colors.BRIGHT_CYAN)}[KAYIT] Tamamlandı.{Colors.RESET}")

    print(f"{Colors.get_color(Colors.BRIGHT_YELLOW)}[İŞLEME] Ses işleniyor...{Colors.RESET}")
    processed_1d = librosa.effects.pitch_shift(recording.flatten(), sr=SAMPLE_RATE, n_steps=-4)
    noise = np.random.normal(0, 0.005, processed_1d.shape).astype(DTYPE)
    processed_1d = processed_1d + noise
    processed_1d = np.clip(processed_1d, -1.0, 1.0)

    print(f"{Colors.get_color(Colors.BRIGHT_MAGENTA)}[OYNATMA] İşlenmiş ses çalınıyor...{Colors.RESET}")
    sd.play(processed_1d, SAMPLE_RATE)
    sd.wait()
    print(f"{Colors.get_color(Colors.BRIGHT_MAGENTA)}[OYNATMA] Tamamlandı.{Colors.RESET}")

    try:
        import soundfile as sf
        sf.write(RECORDING_FILENAME, processed_1d, SAMPLE_RATE)
        print(f"{Colors.get_color(Colors.BRIGHT_GREEN)}[KAYIT] İşlenmiş ses '{RECORDING_FILENAME}' dosyasına kaydedildi.{Colors.RESET}")
    except ImportError:
        print(f"{Colors.get_color(Colors.RED)}[HATA] soundfile kütüphanesi yüklü değil. 'pip install soundfile' ile yükleyiniz. Kayıt yapılamadı.{Colors.RESET}")
    except Exception as e:
        print(f"{Colors.get_color(Colors.RED)}[HATA] Ses kaydedilirken bir sorun oluştu: {e}{Colors.RESET}")

# ==================
# 2. GERÇEK ZAMANLI Mod
# ==================

input_q = queue.Queue()
output_q = queue.Queue()

def audio_input_callback(indata, frames, time_info, status):
    if status:
        print(f"{Colors.get_color(Colors.YELLOW)}[Uyarı-Giriş]{Colors.RESET} {status}")
    input_q.put(indata.copy())

def audio_output_callback(outdata, frames, time_info, status):
    if status:
        print(f"{Colors.get_color(Colors.YELLOW)}[Uyarı-Çıkış]{Colors.RESET} {status}")
    try:
        data = output_q.get_nowait()
        if len(data) >= len(outdata):
            outdata[:] = data[:len(outdata)]
            if len(data) > len(outdata):
                output_q.put_nowait(data[len(outdata):])
        else:
            outdata.fill(0)
    except queue.Empty:
        outdata.fill(0)

def realtime_processor():
    print(f"{Colors.get_color(Colors.BRIGHT_BLUE)}[GERÇEK ZAMANLI] Ses İşleyici Başlatıldı.{Colors.RESET}")
    while True:
        input_data = input_q.get()
        processed_1d = librosa.effects.pitch_shift(input_data.flatten(), sr=SAMPLE_RATE, n_steps=-4)
        noise = np.random.normal(0, 0.003, processed_1d.shape).astype(DTYPE)
        processed_1d = processed_1d + noise
        processed_1d = np.clip(processed_1d, -1.0, 1.0)
        processed_2d = processed_1d.reshape(-1, CHANNELS)
        output_q.put(processed_2d)

def realtime_mode():
    print(f"{Colors.get_color(Colors.BRIGHT_GREEN)}{Colors.BOLD}*** GERÇEK ZAMANLI SES DEĞİŞTİRME MODU ***{Colors.RESET}")
    print(f"{Colors.get_color(Colors.BRIGHT_WHITE)}Mikrofon → Anonim Ses (Çıkmak için {Colors.RED}Ctrl+C{Colors.BRIGHT_WHITE})")

    processor_thread = threading.Thread(target=realtime_processor, daemon=True)
    processor_thread.start()

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
        print(f"{Colors.get_color(Colors.BRIGHT_GREEN)}Gerçek zamanlı akış başladı... Konuşun ve işlenmiş sesi duyun.{Colors.RESET}")
        while input_stream.is_active and output_stream.is_active:
            time.sleep(0.1)
        print(f"{Colors.get_color(Colors.RED)}Akış durduruldu.{Colors.RESET}")

# ==================
# Ana Menü
# ==================

def display_menu():
    c = Colors # Kodu daha kısa yazmak için kısaltma
    print(f"\n{c.get_color(c.BRIGHT_YELLOW)}{c.BOLD}{'='*40}{c.RESET}")
    print(f"{c.get_color(c.BRIGHT_CYAN)}{c.BOLD}{' ':10}SES İŞLEME UYGULAMASI{' ':10}{c.RESET}")
    print(f"{c.get_color(c.BRIGHT_YELLOW)}{c.BOLD}{'='*40}{c.RESET}")

    print(f"{c.get_color(c.BRIGHT_WHITE)} {c.BOLD}Seçenekler:{c.RESET}")
    print(f" {c.get_color(c.GREEN)}{c.BOLD}1.{c.RESET} {c.BRIGHT_WHITE}Kaydet, İşle, Dinle ve Karışık Hali Kaydet {c.RESET}{c.get_color(c.BRIGHT_BLACK)}(5 sn kayıt){c.RESET}")
    print(f" {c.get_color(c.BLUE)}{c.BOLD}2.{c.RESET} {c.BRIGHT_WHITE}Gerçek Zamanlı Ses Değiştirme {c.RESET}{c.get_color(c.BRIGHT_BLACK)}(Mikrofon → Kulaklık){c.RESET}")
    print(f" {c.get_color(c.RED)}{c.BOLD}3.{c.RESET} {c.BRIGHT_WHITE}Çıkış{c.RESET}")
    print(f"{c.get_color(c.BRIGHT_YELLOW)}{c.BOLD}{'='*40}{c.RESET}")

def main():
    while True:
        display_menu()
        choice = input(f"{Colors.get_color(Colors.BRIGHT_WHITE)}Lütfen bir seçenek girin {Colors.get_color(Colors.CYAN)}(1-3){Colors.BRIGHT_WHITE}: {Colors.RESET}")

        if choice == '1':
            print(f"\n{Colors.get_color(Colors.MAGENTA)}{Colors.BOLD}>>> Seçim: Kaydet & İşle Modu <<<{Colors.RESET}\n")
            record_process_play_save_mode()
            print(f"\n{Colors.get_color(Colors.BRIGHT_YELLOW)}{'='*50}{Colors.RESET}\n")
        elif choice == '2':
            print(f"\n{Colors.get_color(Colors.MAGENTA)}{Colors.BOLD}>>> Seçim: Gerçek Zamanlı Mod <<<{Colors.RESET}\n")
            try:
                realtime_mode()
            except KeyboardInterrupt:
                print(f"\n{Colors.get_color(Colors.RED)}Gerçek zamanlı moddan çıkılıyor.{Colors.RESET}")
            except Exception as e:
                print(f"{Colors.get_color(Colors.RED)}[HATA] Bir hata oluştu: {e}{Colors.RESET}")
            print(f"\n{Colors.get_color(Colors.BRIGHT_YELLOW)}{'='*50}{Colors.RESET}\n")
        elif choice == '3':
            print(f"{Colors.get_color(Colors.BRIGHT_RED)}Programdan çıkılıyor. Hoşça kalın!{Colors.RESET}")
            break
        else:
            print(f"{Colors.get_color(Colors.RED)}Geçersiz seçenek. Lütfen {Colors.CYAN}1, 2{Colors.RED} veya {Colors.RED}3{Colors.RESET}{Colors.RED} girin.{Colors.RESET}")
            time.sleep(1)

if __name__ == "__main__":
    main()