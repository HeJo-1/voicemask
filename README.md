# Lütfen Bir Dil seçin / Please select a language
[Türkçe](#gerçek-zamanlı-ses-değiştirici-ve-i̇şleyici) / [English](#english)


# Gerçek Zamanlı Ses Değiştirici ve İşleyici
Bu proje, C ve Python dillerinde yazılmış, mikrofon girdisini gerçek zamanlı olarak işleyerek veya bir ses dosyasını kaydedip işleyerek sesin perdesini (pitch) değiştiren ve anonimleştiren bir komut satırı uygulamasıdır. Her iki implementasyon da aynı temel ses işleme mantığını paylaşır.

![alt text](https://img.shields.io/badge/Language-C-blue.svg)
![alt text](https://img.shields.io/badge/Language-Python-yellow.svg)
![alt text](https://img.shields.io/badge/License-MIT-green.svg)

## 🌟 Temel Özellikler
### İki Farklı Çalışma Modu:
 Kayıt ve İşleme Modu:  Belirtilen süre boyunca sesi kaydeder, sesin perdesini düşürür, hafif bir gürültü ekler, sonucu çalar ve son olarak işlenmiş sesi bir .wav dosyasına kaydeder.

 
 Gerçek Zamanlı Mod: Mikrofondan gelen sesi anlık olarak işler ve doğrudan kulaklığınıza veya hoparlörünüze verir. Bu mod, sesinizi anında anonimleştirmek için idealdir.
## Ses İşleme:
**Pitch Shifting (Perde Kaydırma):**  Sesin perdesini düşürerek daha kalın bir ses elde eder.

**Gürültü Ekleme:** Sese çok hafif, rastgele bir gürültü ekleyerek tanınabilirliği daha da azaltır.

**Çoklu Platform Desteği:** PortAudio (C için) ve sounddevice (Python için) kütüphaneleri sayesinde Linux, macOS ve Windows üzerinde çalışabilir.

**Paralel İşleme (C Versiyonu):** Gerçek zamanlı modda, ses kaydı ve ses işleme görevleri, pthreads kullanılarak iki ayrı thread üzerinde çalışır. Bu, gecikmeyi en aza indirir ve akıcı bir 
deneyim sağlar.

🖥️ Arayüz Görüntüsü
Uygulama başlatıldığında sizi basit ve renkli bir menü karşılar:
<img width="847" height="215" alt="image" src="https://github.com/user-attachments/assets/0a1d0082-45c6-4644-af96-81c0082f5fae" />

## 🛠️ Teknik Detaylar
Her iki versiyon da (C ve Python) aynı ses işleme akışını takip eder, ancak altta yatan teknolojiler farklıdır.

### Ses İşleme Akışı

Ses Verisi Alımı: Ses, ya bir dosyadan okunur ya da mikrofondan canlı olarak alınır. Veri, float (kayan noktalı sayı) dizisi olarak temsil edilir.


Pitch Shifting (Perde Kaydırma): simple_pitch_shift fonksiyonu, temel bir yeniden örnekleme (resampling) tekniği kullanır. Sesi yavaşlatarak veya hızlandırarak perdeyi değiştirir. Bizim
durumumuzda, ses verisini "uzatarak" daha düşük frekanslı (daha kalın) bir ses elde ederiz.


Gürültü Ekleme: Sinyalin tanınmasını zorlaştırmak için her bir ses örneğine (-0.003 ile +0.003 arasında) çok küçük, rastgele bir değer eklenir.


Kırpma (Clipping): İşlem sonrası sinyal genliği -1.0 ve 1.0 aralığının dışına çıkarsa, bu aralığa geri kırpılır. Bu, hoparlörde "patlama" seslerini önler.


### Gerçek Zamanlı Mod Mimarisi (C Versiyonu)
C versiyonundaki gerçek zamanlı mod, Üretici-Tüketici (Producer-Consumer) modelini kullanır ve iki thread'den oluşur:

Ana Thread (PortAudio Callback):

Üretici: PortAudio kütüphanesi tarafından yönetilen bu thread, mikrofondan düzenli aralıklarla ses verisi bloklarını (FRAMES_PER_BUFFER) alır ve bunları bir inputBuffer (giriş tamponu) içine yazar.

Tüketici: Aynı zamanda, işlenmiş ses verilerini bir outputBuffer (çıkış tamponu) içerisinden okur ve ses kartına (kulaklığa) gönderir.

İşleyici Thread (realtime_processor_thread):

Tüketici: Sürekli olarak inputBuffer'ı kontrol eder. Yeni veri geldiğinde, bu veriyi okur.

Üretici: Okuduğu veri bloğuna pitch shift ve gürültü ekleme algoritmalarını uygular. Elde ettiği işlenmiş veriyi outputBuffer'a yazar.

Bu iki thread arasındaki veri alışverişi, pthread_mutex (karşılıklı dışlama) ve pthread_cond (koşul değişkenleri) ile senkronize edilir. Bu, aynı anda buffer'a yazma ve okuma işlemlerinin çakışmasını (race condition) engeller.



## 🚀 Kurulum ve Başlatma
Projeyi çalıştırmak için aşağıdaki adımları izleyin.
1. Depoyu Klonlayın

```bash
git clone https://github.com/HeJo-1/voicemask.git
cd voicemask
```

2. C Versiyonu İçin Kurulum

#### Bağımlılıklar

Öncelikle sisteminizde gcc derleyicisi ve gerekli ses kütüphanelerinin kurulu olması gerekir.

Debian/Ubuntu için:
```bash
sudo apt-get update
sudo apt-get install build-essential portaudio19-dev libsndfile1-dev
```

Diğer sistemler için: (Örn: brew install portaudio libsndfile macOS'te) kendi paket yöneticinizi kullanarak portaudio ve libsndfile kütüphanelerini kurun.


Derleme ve Çalıştırma
```bash
gcc tr.c -o audio_app -lportaudio -lsndfile -lm -lpthread
```
 Uygulamayı çalıştırın
 ```bash
./audio_app
```


3. Python Versiyonu İçin Kurulum
   
Bağımlılıklar

Python 3 ve pip paket yöneticisinin kurulu olduğundan emin olun. Gerekli kütüphaneleri kurmak için:

``` bash
pip install sounddevice soundfile numpy
```
Çalıştırma

```bash
python3 tr.py
```

## GUI 
### Linux
#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install python3-tk
pip3 install cutomtkinter
python3 gui.py
```
#### Fedora/Red Hat
```bash
sudo dnf install python3-tkinter
pip3 install cutomtkinter
python3 gui.py
```
### Windows
```bash
pip install cutomtkinter
```
<img width="603" height="430" alt="image" src="https://github.com/user-attachments/assets/2ec0e720-3624-4589-88de-b084967c510d" />



## Örnek
[kaydedilen_karisik_ses.wav](https://github.com/user-attachments/files/22196529/kaydedilen_karisik_ses.wav)

## 🤝 Katkıda Bulunma
Katkılarınız bizim için değerlidir! Hata düzeltmeleri, yeni özellikler veya iyileştirmeler için Pull Request göndermekten çekinmeyin.

Projeyi Fork'layın.

Yeni bir Feature Branch oluşturun (git checkout -b ozellik/yeni-bir-sey).

Değişikliklerinizi Commit'leyin (git commit -m 'Yeni bir özellik eklendi').

Branch'inizi Push'layın (git push origin ozellik/yeni-bir-sey).

Bir Pull Request açın.

## 📄 Lisans
Bu proje MIT Lisansı altında lisanslanmıştır. Detaylar için LICENSE dosyasına göz atabilirsiniz. (Not: Projenize bir LISENCE dosyası eklemeniz önerilir.)



# English



# Real-Time Voice Changer and Processor
This project is a command-line application written in C and Python that changes the pitch of a voice and anonymizes it, either by processing microphone input in real-time or by recording and then processing an audio file. Both implementations share the same core audio processing logic.

![alt text](https://img.shields.io/badge/Language-C-blue.svg)![alt text](https://img.shields.io/badge/Language-Python-yellow.svg)![alt text](https://img.shields.io/badge/License-MIT-green.svg)

## 🌟 Core Features
### Two Different Operating Modes:
 **Record and Process Mode:** Records audio for a specified duration, lowers the pitch of the voice, adds slight noise, plays the result, and finally saves the processed audio to a .wav file.

 **Real-Time Mode:** Processes the audio from the microphone in real-time and outputs it directly to your headphones or speakers. This mode is ideal for instantly anonymizing your voice.
## Audio Processing:
**Pitch Shifting:** Lowers the pitch of the voice to achieve a deeper sound.

**Noise Addition:** Adds very light, random noise to the audio to further reduce recognizability.

**Multi-Platform Support:** Thanks to the PortAudio (for C) and sounddevice (for Python) libraries, it can run on Linux, macOS, and Windows.

**Parallel Processing (C Version):** In real-time mode, the audio recording and audio processing tasks run on two separate threads using pthreads. This minimizes latency and provides a smooth experience.

🖥️ Interface Screenshot
When the application starts, you are greeted with a simple and colorful menu:
<img width="847" height="215" alt="image" src="https://github.com/user-attachments/assets/0a1d0082-45c6-4644-af96-81c0082f5fae" />

## 🛠️ Technical Details
Both versions (C and Python) follow the same audio processing flow, but the underlying technologies differ.

### Audio Processing Flow

**Audio Data Acquisition:** Audio is either read from a file or captured live from the microphone. The data is represented as an array of floats.

**Pitch Shifting:** The `simple_pitch_shift` function uses a basic resampling technique. It changes the pitch by slowing down or speeding up the audio. In our case, we "stretch" the audio data to obtain a lower frequency (deeper) sound.

**Noise Addition:** To make the signal harder to recognize, a very small, random value (between -0.003 and +0.003) is added to each audio sample.

**Clipping:** If the post-processing signal amplitude goes outside the -1.0 to 1.0 range, it is clipped back into this range. This prevents "popping" sounds on the speakers.

### Real-Time Mode Architecture (C Version)
The real-time mode in the C version uses the Producer-Consumer model and consists of two threads:

**Main Thread (PortAudio Callback):**

**Producer:** This thread, managed by the PortAudio library, receives blocks of audio data (FRAMES_PER_BUFFER) from the microphone at regular intervals and writes them into an `inputBuffer`.

**Consumer:** At the same time, it reads the processed audio data from an `outputBuffer` and sends it to the sound card (headphones).

**Processor Thread (`realtime_processor_thread`):**

**Consumer:** It continuously checks the `inputBuffer`. When new data arrives, it reads it.

**Producer:** It applies the pitch shift and noise addition algorithms to the data block it has read. It then writes the resulting processed data to the `outputBuffer`.

Data exchange between these two threads is synchronized with `pthread_mutex` (mutual exclusion) and `pthread_cond` (condition variables). This prevents race conditions, where writing and reading from the buffer might conflict.

## 🚀 Setup and Launch
Follow the steps below to run the project.
1. Clone the Repository

```bash
git clone https://github.com/HeJo-1/voicemask.git
cd voicemask
```

2. Setup for C Version

#### Dependencies

First, you need to have the `gcc` compiler and the necessary audio libraries installed on your system.

**For Debian/Ubuntu:**
```bash
sudo apt-get update
sudo apt-get install build-essential portaudio19-dev libsndfile1-dev
```

**For other systems:** (e.g., `brew install portaudio libsndfile` on macOS) use your own package manager to install the `portaudio` and `libsndfile` libraries.

#### Compile and Run
```bash
gcc eng.c -o audio_app -lportaudio -lsndfile -lm -lpthread
```
 Run the application
 ```bash
./audio_app
```

3. Setup for Python Version
   
**Dependencies**

Make sure you have Python 3 and the pip package manager installed. To install the required libraries:

``` bash
pip install sounddevice soundfile numpy
```
**Running**

```bash
python3 eng.py
```

## GUI 
### Linux
#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install python3-tk
pip3 install cutomtkinter
python3 gui.py
```
#### Fedora/Red Hat
```bash
sudo dnf install python3-tkinter
pip3 install cutomtkinter
python3 gui.py
```

### Windows
```bash
pip install cutomtkinter
```
<img width="603" height="430" alt="image" src="https://github.com/user-attachments/assets/91784090-a167-4679-9865-4177ebe189a5" />


## Exaple
[kaydedilen_karisik_ses.wav](https://github.com/user-attachments/files/22196530/kaydedilen_karisik_ses.wav)


## 🤝 Contributing
Your contributions are valuable to us! Feel free to submit Pull Requests for bug fixes, new features, or improvements.

1. Fork the project.
2. Create a new Feature Branch (`git checkout -b feature/new-amazing-feature`).
3. Commit your changes (`git commit -m 'Add a new feature'`).
4. Push your Branch (`git push origin feature/new-amazing-feature`).
5. Open a Pull Request.

## 📄 License
This project is licensed under the MIT License. See the `LICENSE` file for details. (Note: It is recommended to add a LICENSE file to your project.)
