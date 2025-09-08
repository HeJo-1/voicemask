import customtkinter as ctk
import threading
import time
import sys
import sounddevice as sd
import numpy as np
import librosa
import queue
import os

try:
    import soundfile as sf
except ImportError:
    sf = None

# ==================
# Genel Ayarlar
# ==================
SAMPLE_RATE = 22050
BLOCK_SIZE = 1024
CHANNELS = 1
DTYPE = 'float32'
RECORDING_FILENAME = "kaydedilen_karisik_ses.wav"

# ==================
# Çoklu Dil (TR / EN)
# ==================
LANG = "tr"

TEXTS = {
    "tr": {
        "title": "Ses İşleme Uygulaması",
        "record_process": "Kaydet, İşle, Dinle ve Kaydet",
        "realtime": "Gerçek Zamanlı Ses Değiştirme",
        "exit": "Çıkış",
        "duration": "Kayıt süresi (sn):",
        "logs": "Durum Mesajları",
        "lang": "Dil: ",
        "processing": "Ses işleniyor...",
        "done": "Tamamlandı.",
        "playing": "İşlenmiş ses çalınıyor...",
        "saved": f"İşlenmiş ses '{RECORDING_FILENAME}' dosyasına kaydedildi.",
        "stream_start": "Gerçek zamanlı akış başladı... Konuşun!",
        "stream_stop": "Akış durduruldu."
    },
    "en": {
        "title": "Audio Processing App",
        "record_process": "Record, Process, Play & Save",
        "realtime": "Real-time Voice Changer",
        "exit": "Exit",
        "duration": "Recording duration (s):",
        "logs": "Log Messages",
        "lang": "Language: ",
        "processing": "Processing audio...",
        "done": "Done.",
        "playing": "Playing processed audio...",
        "saved": f"Processed audio saved as '{RECORDING_FILENAME}'.",
        "stream_start": "Real-time stream started... Speak!",
        "stream_stop": "Stream stopped."
    }
}

# ==================
# Ses İşleme Fonksiyonları
# ==================
def record_process_play_save_mode(duration, log_callback):
    log_callback(TEXTS[LANG]["processing"])
    recording = sd.rec(int(duration * SAMPLE_RATE), samplerate=SAMPLE_RATE, channels=CHANNELS, dtype=DTYPE)
    sd.wait()

    processed_1d = librosa.effects.pitch_shift(recording.flatten(), sr=SAMPLE_RATE, n_steps=-4)
    noise = np.random.normal(0, 0.005, processed_1d.shape).astype(DTYPE)
    processed_1d = np.clip(processed_1d + noise, -1.0, 1.0)

    log_callback(TEXTS[LANG]["playing"])
    sd.play(processed_1d, SAMPLE_RATE)
    sd.wait()
    log_callback(TEXTS[LANG]["done"])

    if sf:
        sf.write(RECORDING_FILENAME, processed_1d, SAMPLE_RATE)
        log_callback(TEXTS[LANG]["saved"])

# Realtime için kuyruqlar
input_q = queue.Queue()
output_q = queue.Queue()

def audio_input_callback(indata, frames, time_info, status):
    input_q.put(indata.copy())

def audio_output_callback(outdata, frames, time_info, status):
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
    while True:
        input_data = input_q.get()
        processed_1d = librosa.effects.pitch_shift(input_data.flatten(), sr=SAMPLE_RATE, n_steps=-4)
        noise = np.random.normal(0, 0.003, processed_1d.shape).astype(DTYPE)
        processed_1d = np.clip(processed_1d + noise, -1.0, 1.0)
        output_q.put(processed_1d.reshape(-1, CHANNELS))

def realtime_mode(log_callback):
    log_callback(TEXTS[LANG]["stream_start"])
    processor_thread = threading.Thread(target=realtime_processor, daemon=True)
    processor_thread.start()

    try:
        with sd.InputStream(callback=audio_input_callback,
                            samplerate=SAMPLE_RATE,
                            channels=CHANNELS,
                            dtype=DTYPE,
                            blocksize=BLOCK_SIZE), \
             sd.OutputStream(callback=audio_output_callback,
                             samplerate=SAMPLE_RATE,
                             channels=CHANNELS,
                             dtype=DTYPE,
                             blocksize=BLOCK_SIZE):
            while True:
                time.sleep(0.1)
    except KeyboardInterrupt:
        log_callback(TEXTS[LANG]["stream_stop"])

# ==================
# GUI
# ==================
class AudioApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title(TEXTS[LANG]["title"])
        self.geometry("600x400")

        # Dil seçimi
        self.lang_var = ctk.StringVar(value=LANG)
        lang_frame = ctk.CTkFrame(self)
        lang_frame.pack(pady=10)
        ctk.CTkLabel(lang_frame, text=TEXTS[LANG]["lang"]).pack(side="left", padx=5)
        lang_menu = ctk.CTkOptionMenu(lang_frame, values=["tr", "en"], variable=self.lang_var, command=self.change_lang)
        lang_menu.pack(side="left")

        # Süre
        self.duration_var = ctk.IntVar(value=5)
        duration_frame = ctk.CTkFrame(self)
        duration_frame.pack(pady=10)
        self.duration_label = ctk.CTkLabel(duration_frame, text=TEXTS[LANG]["duration"])
        self.duration_label.pack(side="left", padx=5)
        self.duration_entry = ctk.CTkEntry(duration_frame, textvariable=self.duration_var, width=60)
        self.duration_entry.pack(side="left")

        # Butonlar
        button_frame = ctk.CTkFrame(self)
        button_frame.pack(pady=10)
        self.btn_record = ctk.CTkButton(button_frame, text=TEXTS[LANG]["record_process"], command=self.run_record)
        self.btn_record.pack(pady=5)
        self.btn_realtime = ctk.CTkButton(button_frame, text=TEXTS[LANG]["realtime"], command=self.run_realtime)
        self.btn_realtime.pack(pady=5)
        self.btn_exit = ctk.CTkButton(button_frame, text=TEXTS[LANG]["exit"], command=self.destroy)
        self.btn_exit.pack(pady=5)

        # Log kutusu
        ctk.CTkLabel(self, text=TEXTS[LANG]["logs"]).pack()
        self.logbox = ctk.CTkTextbox(self, width=500, height=120)
        self.logbox.pack(pady=5)

    def log(self, msg):
        self.logbox.insert("end", msg + "\n")
        self.logbox.see("end")

    def run_record(self):
        duration = self.duration_var.get()
        threading.Thread(target=record_process_play_save_mode, args=(duration, self.log), daemon=True).start()

    def run_realtime(self):
        threading.Thread(target=realtime_mode, args=(self.log,), daemon=True).start()

    def change_lang(self, new_lang):
        global LANG
        LANG = new_lang
        self.title(TEXTS[LANG]["title"])
        self.duration_label.configure(text=TEXTS[LANG]["duration"])
        self.btn_record.configure(text=TEXTS[LANG]["record_process"])
        self.btn_realtime.configure(text=TEXTS[LANG]["realtime"])
        self.btn_exit.configure(text=TEXTS[LANG]["exit"])

if __name__ == "__main__":
    ctk.set_appearance_mode("dark")
    app = AudioApp()
    app.mainloop()
