#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <portaudio.h>
#include <sndfile.h>
#include <pthread.h> // Threading için
#include <unistd.h>  // sleep için

//sudo apt-get update
//sudo apt-get install portaudio19-dev libsndfile1-dev
//gcc audio_app.c -o audio_app -lportaudio -lsndfile -lm -lpthread

// ==================
// ANSI Renk Kodları
// ==================
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define UNDERLINE "\033[4m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BRIGHT_BLACK "\033[90m"
#define BRIGHT_RED "\033[91m"
#define BRIGHT_GREEN "\033[92m"
#define BRIGHT_YELLOW "\033[93m"
#define BRIGHT_BLUE "\033[94m"
#define BRIGHT_MAGENTA "\033[95m"
#define BRIGHT_CYAN "\033[96m"
#define BRIGHT_WHITE "\033[97m"

#define GET_COLOR(color_code) color_code

// ==================
// Genel Ayarlar
// ==================
#define SAMPLE_RATE         44100
#define FRAMES_PER_BUFFER   512
#define NUM_CHANNELS        1
#define RECORDING_FILENAME  "kaydedilen_karisik_ses_c.wav"
#define PITCH_SHIFT_STEPS   -4

// DEĞİŞİKLİK: Sabit DURATION_SECONDS kaldırıldı.

// ==================
// Realtime Data Yapısı
// ==================
typedef struct {
    float *buffer;
    long buffer_size;
    pthread_mutex_t mutex;
    pthread_cond_t cond_data_available;
    pthread_cond_t cond_buffer_empty;
    long read_idx;
    long write_idx;
    bool terminate;
} RealtimeBuffer;

RealtimeBuffer inputBuffer;
RealtimeBuffer outputBuffer;

// ==================
// Fonksiyon Prototipleri
// ==================
void initialize_realtime_buffer(RealtimeBuffer *rb, long size);
void destroy_realtime_buffer(RealtimeBuffer *rb);
bool write_to_buffer(RealtimeBuffer *rb, const float *data, long frames);
bool read_from_buffer(RealtimeBuffer *rb, float *data, long frames);

int paCallback(const void *inputBufferPtr, void *outputBufferPtr,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void *userData);

void *realtime_processor_thread(void *arg);
void simple_pitch_shift(float *data, long num_frames, int sample_rate, int n_steps);
void record_process_play_save_mode();
void realtime_mode();
void display_menu();
void clear_input_buffer();

// ==================
// Main Fonksiyonu
// ==================
int main() {
    PaError err;
    int choice;

    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio hatası: %s\n"RESET, Pa_GetErrorText(err));
        return 1;
    }

    while (true) {
        display_menu();
        printf(GET_COLOR(BRIGHT_WHITE)"Lütfen bir seçenek girin "GET_COLOR(CYAN)"(1-3)"GET_COLOR(BRIGHT_WHITE)": "RESET);

        if (scanf("%d", &choice) != 1) {
            printf(GET_COLOR(RED)"Geçersiz giriş. Lütfen bir sayı girin."RESET"\n");
            clear_input_buffer();
            sleep(1);
            continue;
        }
        clear_input_buffer();

        if (choice == 1) {
            printf(GET_COLOR(MAGENTA)BOLD">>> Seçim: Kaydet & İşle Modu <<<"RESET"\n\n");
            record_process_play_save_mode();
            printf(GET_COLOR(BRIGHT_YELLOW)"%s\n"RESET, "==================================================");
        } else if (choice == 2) {
            printf(GET_COLOR(MAGENTA)BOLD">>> Seçim: Gerçek Zamanlı Mod <<<"RESET"\n\n");
            realtime_mode();
            printf(GET_COLOR(BRIGHT_YELLOW)"%s\n"RESET, "==================================================");
        } else if (choice == 3) {
            printf(GET_COLOR(BRIGHT_RED)"Programdan çıkılıyor. Hoşça kalın!"RESET"\n");
            break;
        } else {
            printf(GET_COLOR(RED)"Geçersiz seçenek. Lütfen "GET_COLOR(CYAN)"1, 2"GET_COLOR(RED)" veya "GET_COLOR(RED)"3"GET_COLOR(RED)" girin."RESET"\n");
            sleep(1);
        }
    }

    err = Pa_Terminate();
    if (err != paNoError) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio hatası: %s\n"RESET, Pa_GetErrorText(err));
    }

    return 0;
}


// ==================
// 1. KAYDET → İŞLE → DİNLE → KAYDET Modu Fonksiyonu
// ==================
void record_process_play_save_mode() {
    PaStream *stream = NULL;
    PaError err;
    int duration_seconds; // DEĞİŞİKLİK: Süre için yerel değişken

    // DEĞİŞİKLİK: Kullanıcıdan kayıt süresini al
    printf(GET_COLOR(BRIGHT_WHITE)"Lütfen kayıt süresini saniye cinsinden girin: "RESET);
    if (scanf("%d", &duration_seconds) != 1) {
        printf(GET_COLOR(RED)"Geçersiz giriş. Lütfen bir sayı girin."RESET"\n");
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    if (duration_seconds <= 0) {
        printf(GET_COLOR(RED)"Geçersiz süre. Süre 0'dan büyük olmalıdır."RESET"\n");
        return;
    }

    long num_frames = SAMPLE_RATE * duration_seconds * NUM_CHANNELS;
    float *recorded_samples = (float*) calloc(num_frames, sizeof(float));

    if (!recorded_samples) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Bellek ayırma hatası!\n"RESET);
        return;
    }

    printf(GET_COLOR(BRIGHT_CYAN)"[KAYIT] Konuşmaya başlayın (%d saniye)..."RESET"\n", duration_seconds);

    err = Pa_OpenDefaultStream(&stream, NUM_CHANNELS, 0, paFloat32, SAMPLE_RATE, paFramesPerBufferUnspecified, NULL, NULL);
    if (err != paNoError) goto error_record;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error_record;

    err = Pa_ReadStream(stream, recorded_samples, num_frames);
    if (err != paNoError) goto error_record;

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error_record;
    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error_record;
    stream = NULL;

    printf(GET_COLOR(BRIGHT_CYAN)"[KAYIT] Tamamlandı.\n"RESET);
    printf(GET_COLOR(BRIGHT_YELLOW)"[İŞLEME] Ses işleniyor...\n"RESET);

    simple_pitch_shift(recorded_samples, num_frames, SAMPLE_RATE, PITCH_SHIFT_STEPS);

    for (long i = 0; i < num_frames; ++i) {
        float noise = ((float)rand() / (float)RAND_MAX) * 0.006f - 0.003f;
        recorded_samples[i] += noise;
        if (recorded_samples[i] > 1.0f) recorded_samples[i] = 1.0f;
        if (recorded_samples[i] < -1.0f) recorded_samples[i] = -1.0f;
    }

    printf(GET_COLOR(BRIGHT_MAGENTA)"[OYNATMA] İşlenmiş ses çalınıyor...\n"RESET);

    err = Pa_OpenDefaultStream(&stream, 0, NUM_CHANNELS, paFloat32, SAMPLE_RATE, paFramesPerBufferUnspecified, NULL, NULL);
    if (err != paNoError) goto error_play;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error_play;

    err = Pa_WriteStream(stream, recorded_samples, num_frames);
    if (err != paNoError) goto error_play;

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error_play;
    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error_play;
    stream = NULL;

    printf(GET_COLOR(BRIGHT_MAGENTA)"[OYNATMA] Tamamlandı.\n"RESET);

    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(SF_INFO));
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.channels = NUM_CHANNELS;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    SNDFILE *outfile = sf_open(RECORDING_FILENAME, SFM_WRITE, &sfinfo);
    if (!outfile) {
        fprintf(stderr, GET_COLOR(RED)"[HATA] '%s' dosyası açılamadı: %s\n"RESET, RECORDING_FILENAME, sf_strerror(NULL));
    } else {
        sf_write_float(outfile, recorded_samples, num_frames);
        sf_close(outfile);
        printf(GET_COLOR(BRIGHT_GREEN)"[KAYIT] İşlenmiş ses '%s' dosyasına kaydedildi.\n"RESET, RECORDING_FILENAME);
    }

    free(recorded_samples);
    return;

error_record:
    if (stream) Pa_CloseStream(stream);
    fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio Kayıt Hatası: %s\n"RESET, Pa_GetErrorText(err));
    free(recorded_samples);
    return;
error_play:
    if (stream) Pa_CloseStream(stream);
    fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio Çalma Hatası: %s\n"RESET, Pa_GetErrorText(err));
    free(recorded_samples);
    return;
}

// ==================
// Ana Menü Fonksiyonu
// ==================
void display_menu() {
    printf("\n%s%s%s\n", GET_COLOR(BRIGHT_YELLOW), BOLD, "========================================"RESET);
    printf("%s%s  SES İŞLEME UYGULAMASI %s\n", GET_COLOR(BRIGHT_CYAN), BOLD, RESET);
    printf("%s%s%s\n", GET_COLOR(BRIGHT_YELLOW), BOLD, "========================================"RESET);
    printf("%s %sSeçenekler:%s\n", GET_COLOR(BRIGHT_WHITE), BOLD, RESET);
    // DEĞİŞİKLİK: "(5 sn kayıt)" ifadesi kaldırıldı
    printf(" %s%s1.%s %sKaydet, İşle, Dinle ve Karışık Hali Kaydet\n",
           GET_COLOR(GREEN), BOLD, RESET, GET_COLOR(BRIGHT_WHITE));
    printf(" %s%s2.%s %sGerçek Zamanlı Ses Değiştirme %s(Mikrofon → Kulaklık)%s\n",
           GET_COLOR(BLUE), BOLD, RESET, GET_COLOR(BRIGHT_WHITE), GET_COLOR(BRIGHT_BLACK), RESET);
    printf(" %s%s3.%s %sÇıkış%s\n",
           GET_COLOR(RED), BOLD, RESET, GET_COLOR(BRIGHT_WHITE), RESET);
    printf("%s%s%s\n", GET_COLOR(BRIGHT_YELLOW), BOLD, "========================================"RESET);
}


// --- DİĞER FONKSİYONLAR (Değişiklik yapılmadı) ---

// ==================
// Realtime Buffer Fonksiyonları
// ==================
void initialize_realtime_buffer(RealtimeBuffer *rb, long size) {
    rb->buffer = (float*) calloc(size, sizeof(float));
    if (!rb->buffer) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Bellek ayırma hatası!\n"RESET);
        exit(EXIT_FAILURE);
    }
    rb->buffer_size = size;
    rb->read_idx = 0;
    rb->write_idx = 0;
    rb->terminate = false;
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->cond_data_available, NULL);
    pthread_cond_init(&rb->cond_buffer_empty, NULL);
}

void destroy_realtime_buffer(RealtimeBuffer *rb) {
    free(rb->buffer);
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->cond_data_available);
    pthread_cond_destroy(&rb->cond_buffer_empty);
}

bool write_to_buffer(RealtimeBuffer *rb, const float *data, long frames) {
    pthread_mutex_lock(&rb->mutex);
    long current_data_size = (rb->write_idx - rb->read_idx + rb->buffer_size) % rb->buffer_size;
    long free_space = rb->buffer_size - current_data_size - 1;

    while (free_space < frames && !rb->terminate) {
        pthread_cond_wait(&rb->cond_buffer_empty, &rb->mutex);
        current_data_size = (rb->write_idx - rb->read_idx + rb->buffer_size) % rb->buffer_size;
        free_space = rb->buffer_size - current_data_size - 1;
    }

    if (rb->terminate) {
        pthread_mutex_unlock(&rb->mutex);
        return false;
    }

    for (long i = 0; i < frames; ++i) {
        rb->buffer[rb->write_idx] = data[i];
        rb->write_idx = (rb->write_idx + 1) % rb->buffer_size;
    }

    pthread_cond_signal(&rb->cond_data_available);
    pthread_mutex_unlock(&rb->mutex);
    return true;
}

bool read_from_buffer(RealtimeBuffer *rb, float *data, long frames) {
    pthread_mutex_lock(&rb->mutex);
    long current_data_size = (rb->write_idx - rb->read_idx + rb->buffer_size) % rb->buffer_size;

    while (current_data_size < frames && !rb->terminate) {
        pthread_cond_wait(&rb->cond_data_available, &rb->mutex);
        current_data_size = (rb->write_idx - rb->read_idx + rb->buffer_size) % rb->buffer_size;
    }

    if (rb->terminate) {
        pthread_mutex_unlock(&rb->mutex);
        return false;
    }

    for (long i = 0; i < frames; ++i) {
        data[i] = rb->buffer[rb->read_idx];
        rb->read_idx = (rb->read_idx + 1) % rb->buffer_size;
    }

    pthread_cond_signal(&rb->cond_buffer_empty);
    pthread_mutex_unlock(&rb->mutex);
    return true;
}

// ==================
// PortAudio Callback Fonksiyonu
// ==================
int paCallback(const void *inputBufferPtr, void *outputBufferPtr,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void *userData) {
    float *out = (float*)outputBufferPtr;
    const float *in = (const float*)inputBufferPtr;

    if (statusFlags & paInputOverflow) fprintf(stderr, GET_COLOR(YELLOW)"[Uyarı-Giriş] Giriş taşması.\n"RESET);
    if (statusFlags & paOutputUnderflow) fprintf(stderr, GET_COLOR(YELLOW)"[Uyarı-Çıkış] Çıkış yetersizliği.\n"RESET);

    if (inputBufferPtr != NULL) {
        if (!write_to_buffer(&inputBuffer, in, framesPerBuffer * NUM_CHANNELS)) {
            return paComplete;
        }
    }

    if (outputBufferPtr != NULL) {
        if (!read_from_buffer(&outputBuffer, out, framesPerBuffer * NUM_CHANNELS)) {
            return paComplete;
        }
    } else {
        for (unsigned int i = 0; i < framesPerBuffer * NUM_CHANNELS; i++) {
            out[i] = 0;
        }
    }

    if (inputBuffer.terminate || outputBuffer.terminate) {
         return paComplete;
    }

    return paContinue;
}

// ==================
// Realtime Processor Thread Fonksiyonu
// ==================
void *realtime_processor_thread(void *arg) {
    (void)arg;
    printf(GET_COLOR(BRIGHT_BLUE)"[GERÇEK ZAMANLI] Ses İşleyici Başlatıldı.\n"RESET);

    float *input_block = (float*) malloc(FRAMES_PER_BUFFER * NUM_CHANNELS * sizeof(float));
    float *processed_block = (float*) malloc(FRAMES_PER_BUFFER * NUM_CHANNELS * sizeof(float));
    if (!input_block || !processed_block) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Bellek ayırma hatası!\n"RESET);
        exit(EXIT_FAILURE);
    }

    while (!inputBuffer.terminate) {
        if (!read_from_buffer(&inputBuffer, input_block, FRAMES_PER_BUFFER * NUM_CHANNELS)) {
            break;
        }

        simple_pitch_shift(input_block, FRAMES_PER_BUFFER * NUM_CHANNELS, SAMPLE_RATE, PITCH_SHIFT_STEPS);

        for (unsigned int i = 0; i < FRAMES_PER_BUFFER * NUM_CHANNELS; ++i) {
            float noise = ((float)rand() / (float)RAND_MAX) * 0.006f - 0.003f;
            processed_block[i] = input_block[i] + noise;
            if (processed_block[i] > 1.0f) processed_block[i] = 1.0f;
            if (processed_block[i] < -1.0f) processed_block[i] = -1.0f;
        }

        if (!write_to_buffer(&outputBuffer, processed_block, FRAMES_PER_BUFFER * NUM_CHANNELS)) {
            break;
        }
    }

    free(input_block);
    free(processed_block);
    printf(GET_COLOR(BRIGHT_BLUE)"[GERÇEK ZAMANLI] Ses İşleyici Sonlandırıldı.\n"RESET);
    return NULL;
}

// ==================
// Basit Pitch Shift Fonksiyonu
// ==================
void simple_pitch_shift(float *data, long num_frames, int sample_rate, int n_steps) {
    float pitch_factor = powf(2.0f, (float)n_steps / 12.0f);
    float current_sample_pos = 0.0f;
    float *temp_buffer = (float*) calloc(num_frames, sizeof(float));
    if (!temp_buffer) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Bellek ayırma hatası! Pitch shift yapılamadı.\n"RESET);
        return;
    }

    for (long i = 0; i < num_frames; ++i) {
        long idx1 = (long)floorf(current_sample_pos);
        long idx2 = idx1 + 1;
        float fraction = current_sample_pos - idx1;

        if (idx2 >= num_frames) {
            temp_buffer[i] = data[idx1];
        } else {
            temp_buffer[i] = data[idx1] * (1.0f - fraction) + data[idx2] * fraction;
        }

        current_sample_pos += pitch_factor;

        if (current_sample_pos >= num_frames - 1) {
            for (long j = i + 1; j < num_frames; ++j) {
                temp_buffer[j] = 0.0f;
            }
            break;
        }
    }
    memcpy(data, temp_buffer, num_frames * sizeof(float));
    free(temp_buffer);
}

// ==================
// Mod 2: Gerçek Zamanlı
// ==================
void realtime_mode() {
    PaStream *stream = NULL;
    PaError err;
    pthread_t processor_tid;
    long buffer_frames = SAMPLE_RATE * 2 * NUM_CHANNELS;

    initialize_realtime_buffer(&inputBuffer, buffer_frames);
    initialize_realtime_buffer(&outputBuffer, buffer_frames);

    printf(GET_COLOR(BRIGHT_GREEN)BOLD"*** GERÇEK ZAMANLI SES DEĞİŞTİRME MODU ***"RESET"\n");
    printf(GET_COLOR(BRIGHT_WHITE)"Mikrofon → Anonim Ses (Çıkmak için "GET_COLOR(RED)"Ctrl+C"GET_COLOR(BRIGHT_WHITE)")\n"RESET);

    if (pthread_create(&processor_tid, NULL, realtime_processor_thread, NULL) != 0) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Thread oluşturma hatası!\n"RESET);
        goto cleanup_realtime;
    }

    err = Pa_OpenDefaultStream(&stream, NUM_CHANNELS, NUM_CHANNELS, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, paCallback, NULL);
    if (err != paNoError) goto error_realtime;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error_realtime;

    printf(GET_COLOR(BRIGHT_GREEN)"Gerçek zamanlı akış başladı... Konuşun ve işlenmiş sesi duyun.\n"RESET);

    while (Pa_IsStreamActive(stream) == 1) {
        sleep(1);
    }

    printf(GET_COLOR(BRIGHT_RED)"Akış durduruldu.\n"RESET);

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error_realtime;
    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error_realtime;
    stream = NULL;

error_realtime:
    if (err != paNoError && err != paStreamIsStopped) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio Gerçek Zamanlı Hata: %s\n"RESET, Pa_GetErrorText(err));
    }
    if (stream) {
        Pa_AbortStream(stream);
        Pa_CloseStream(stream);
    }

    inputBuffer.terminate = true;
    outputBuffer.terminate = true;
    pthread_cond_broadcast(&inputBuffer.cond_data_available);
    pthread_cond_broadcast(&inputBuffer.cond_buffer_empty);
    pthread_cond_broadcast(&outputBuffer.cond_data_available);
    pthread_cond_broadcast(&outputBuffer.cond_buffer_empty);

    pthread_join(processor_tid, NULL);

cleanup_realtime:
    destroy_realtime_buffer(&inputBuffer);
    destroy_realtime_buffer(&outputBuffer);
}


// ==================
// Yardımcı Fonksiyon: clear_input_buffer
// ==================
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}