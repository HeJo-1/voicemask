#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <portaudio.h>
#include <sndfile.h>
#include <pthread.h> // For threading
#include <unistd.h>  // for sleep

// On Debian/Ubuntu based systems, you might need to install the following packages:
// sudo apt-get update
// sudo apt-get install portaudio19-dev libsndfile1-dev
//
// To compile the code:
// gcc audio_app.c -o audio_app -lportaudio -lsndfile -lm -lpthread

// ==================
// ANSI Color Codes
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
// General Settings
// ==================
#define SAMPLE_RATE         44100
#define FRAMES_PER_BUFFER   512
#define NUM_CHANNELS        1
#define RECORDING_FILENAME  "recorded_mixed_audio_c.wav"
#define PITCH_SHIFT_STEPS   -4

// CHANGE: Constant DURATION_SECONDS removed.

// ==================
// Realtime Data Structure
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
// Function Prototypes
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
// Main Function
// ==================
int main() {
    PaError err;
    int choice;

    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio error: %s\n"RESET, Pa_GetErrorText(err));
        return 1;
    }

    while (true) {
        display_menu();
        printf(GET_COLOR(BRIGHT_WHITE)"Please enter an option "GET_COLOR(CYAN)"(1-3)"GET_COLOR(BRIGHT_WHITE)": "RESET);

        if (scanf("%d", &choice) != 1) {
            printf(GET_COLOR(RED)"Invalid input. Please enter a number."RESET"\n");
            clear_input_buffer();
            sleep(1);
            continue;
        }
        clear_input_buffer();

        if (choice == 1) {
            printf(GET_COLOR(MAGENTA)BOLD">>> Selection: Record & Process Mode <<<"RESET"\n\n");
            record_process_play_save_mode();
            printf(GET_COLOR(BRIGHT_YELLOW)"%s\n"RESET, "==================================================");
        } else if (choice == 2) {
            printf(GET_COLOR(MAGENTA)BOLD">>> Selection: Realtime Mode <<<"RESET"\n\n");
            realtime_mode();
            printf(GET_COLOR(BRIGHT_YELLOW)"%s\n"RESET, "==================================================");
        } else if (choice == 3) {
            printf(GET_COLOR(BRIGHT_RED)"Exiting the program. Goodbye!"RESET"\n");
            break;
        } else {
            printf(GET_COLOR(RED)"Invalid option. Please enter "GET_COLOR(CYAN)"1, 2"GET_COLOR(RED)" or "GET_COLOR(RED)"3"GET_COLOR(RED)"."RESET"\n");
            sleep(1);
        }
    }

    err = Pa_Terminate();
    if (err != paNoError) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio error: %s\n"RESET, Pa_GetErrorText(err));
    }

    return 0;
}


// ==================
// 1. RECORD → PROCESS → PLAY → SAVE Mode Function
// ==================
void record_process_play_save_mode() {
    PaStream *stream = NULL;
    PaError err;
    int duration_seconds; // CHANGE: Local variable for duration

    // CHANGE: Get recording duration from the user
    printf(GET_COLOR(BRIGHT_WHITE)"Please enter the recording duration in seconds: "RESET);
    if (scanf("%d", &duration_seconds) != 1) {
        printf(GET_COLOR(RED)"Invalid input. Please enter a number."RESET"\n");
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    if (duration_seconds <= 0) {
        printf(GET_COLOR(RED)"Invalid duration. Duration must be greater than 0."RESET"\n");
        return;
    }

    long num_frames = SAMPLE_RATE * duration_seconds * NUM_CHANNELS;
    float *recorded_samples = (float*) calloc(num_frames, sizeof(float));

    if (!recorded_samples) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Memory allocation error!\n"RESET);
        return;
    }

    printf(GET_COLOR(BRIGHT_CYAN)"[RECORD] Start speaking (%d seconds)..."RESET"\n", duration_seconds);

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

    printf(GET_COLOR(BRIGHT_CYAN)"[RECORD] Completed.\n"RESET);
    printf(GET_COLOR(BRIGHT_YELLOW)"[PROCESSING] Processing audio...\n"RESET);

    simple_pitch_shift(recorded_samples, num_frames, SAMPLE_RATE, PITCH_SHIFT_STEPS);

    for (long i = 0; i < num_frames; ++i) {
        float noise = ((float)rand() / (float)RAND_MAX) * 0.006f - 0.003f;
        recorded_samples[i] += noise;
        if (recorded_samples[i] > 1.0f) recorded_samples[i] = 1.0f;
        if (recorded_samples[i] < -1.0f) recorded_samples[i] = -1.0f;
    }

    printf(GET_COLOR(BRIGHT_MAGENTA)"[PLAYBACK] Playing processed audio...\n"RESET);

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

    printf(GET_COLOR(BRIGHT_MAGENTA)"[PLAYBACK] Completed.\n"RESET);

    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(SF_INFO));
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.channels = NUM_CHANNELS;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    SNDFILE *outfile = sf_open(RECORDING_FILENAME, SFM_WRITE, &sfinfo);
    if (!outfile) {
        fprintf(stderr, GET_COLOR(RED)"[ERROR] Could not open file '%s': %s\n"RESET, RECORDING_FILENAME, sf_strerror(NULL));
    } else {
        sf_write_float(outfile, recorded_samples, num_frames);
        sf_close(outfile);
        printf(GET_COLOR(BRIGHT_GREEN)"[SAVE] Processed audio saved to '%s'.\n"RESET, RECORDING_FILENAME);
    }

    free(recorded_samples);
    return;

error_record:
    if (stream) Pa_CloseStream(stream);
    fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio Recording Error: %s\n"RESET, Pa_GetErrorText(err));
    free(recorded_samples);
    return;
error_play:
    if (stream) Pa_CloseStream(stream);
    fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio Playback Error: %s\n"RESET, Pa_GetErrorText(err));
    free(recorded_samples);
    return;
}

// ==================
// Main Menu Function
// ==================
void display_menu() {
    printf("\n%s%s%s\n", GET_COLOR(BRIGHT_YELLOW), BOLD, "========================================"RESET);
    printf("%s%s    AUDIO PROCESSING APPLICATION    %s\n", GET_COLOR(BRIGHT_CYAN), BOLD, RESET);
    printf("%s%s%s\n", GET_COLOR(BRIGHT_YELLOW), BOLD, "========================================"RESET);
    printf("%s %sOptions:%s\n", GET_COLOR(BRIGHT_WHITE), BOLD, RESET);
    // CHANGE: "(5 sec recording)" text removed
    printf(" %s%s1.%s %sRecord, Process, Listen, and Save\n",
           GET_COLOR(GREEN), BOLD, RESET, GET_COLOR(BRIGHT_WHITE));
    printf(" %s%s2.%s %sRealtime Voice Changer %s(Microphone → Headphones)%s\n",
           GET_COLOR(BLUE), BOLD, RESET, GET_COLOR(BRIGHT_WHITE), GET_COLOR(BRIGHT_BLACK), RESET);
    printf(" %s%s3.%s %sExit%s\n",
           GET_COLOR(RED), BOLD, RESET, GET_COLOR(BRIGHT_WHITE), RESET);
    printf("%s%s%s\n", GET_COLOR(BRIGHT_YELLOW), BOLD, "========================================"RESET);
}


// --- OTHER FUNCTIONS (No changes needed below this line) ---

// ==================
// Realtime Buffer Functions
// ==================
void initialize_realtime_buffer(RealtimeBuffer *rb, long size) {
    rb->buffer = (float*) calloc(size, sizeof(float));
    if (!rb->buffer) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Memory allocation error!\n"RESET);
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
// PortAudio Callback Function
// ==================
int paCallback(const void *inputBufferPtr, void *outputBufferPtr,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void *userData) {
    float *out = (float*)outputBufferPtr;
    const float *in = (const float*)inputBufferPtr;

    if (statusFlags & paInputOverflow) fprintf(stderr, GET_COLOR(YELLOW)"[Warning-Input] Input overflow.\n"RESET);
    if (statusFlags & paOutputUnderflow) fprintf(stderr, GET_COLOR(YELLOW)"[Warning-Output] Output underflow.\n"RESET);

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
// Realtime Processor Thread Function
// ==================
void *realtime_processor_thread(void *arg) {
    (void)arg;
    printf(GET_COLOR(BRIGHT_BLUE)"[REALTIME] Audio Processor Started.\n"RESET);

    float *input_block = (float*) malloc(FRAMES_PER_BUFFER * NUM_CHANNELS * sizeof(float));
    float *processed_block = (float*) malloc(FRAMES_PER_BUFFER * NUM_CHANNELS * sizeof(float));
    if (!input_block || !processed_block) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Memory allocation error!\n"RESET);
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
    printf(GET_COLOR(BRIGHT_BLUE)"[REALTIME] Audio Processor Terminated.\n"RESET);
    return NULL;
}

// ==================
// Simple Pitch Shift Function
// ==================
void simple_pitch_shift(float *data, long num_frames, int sample_rate, int n_steps) {
    float pitch_factor = powf(2.0f, (float)n_steps / 12.0f);
    float current_sample_pos = 0.0f;
    float *temp_buffer = (float*) calloc(num_frames, sizeof(float));
    if (!temp_buffer) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Memory allocation error! Pitch shift could not be performed.\n"RESET);
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
// Mode 2: Realtime
// ==================
void realtime_mode() {
    PaStream *stream = NULL;
    PaError err;
    pthread_t processor_tid;
    long buffer_frames = SAMPLE_RATE * 2 * NUM_CHANNELS;

    initialize_realtime_buffer(&inputBuffer, buffer_frames);
    initialize_realtime_buffer(&outputBuffer, buffer_frames);

    printf(GET_COLOR(BRIGHT_GREEN)BOLD"*** REALTIME VOICE CHANGER MODE ***"RESET"\n");
    printf(GET_COLOR(BRIGHT_WHITE)"Microphone → Anonymized Voice (Press "GET_COLOR(RED)"Ctrl+C"GET_COLOR(BRIGHT_WHITE)" to exit)\n"RESET);

    if (pthread_create(&processor_tid, NULL, realtime_processor_thread, NULL) != 0) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"Thread creation error!\n"RESET);
        goto cleanup_realtime;
    }

    err = Pa_OpenDefaultStream(&stream, NUM_CHANNELS, NUM_CHANNELS, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, paCallback, NULL);
    if (err != paNoError) goto error_realtime;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error_realtime;

    printf(GET_COLOR(BRIGHT_GREEN)"Realtime stream started... Speak to hear the processed sound.\n"RESET);

    // In a real application, you would handle signals (like Ctrl+C) gracefully.
    // For this simple example, the user must manually stop it.
    // We wait here until the stream is no longer active.
    while (Pa_IsStreamActive(stream) == 1) {
        sleep(1);
    }

    printf(GET_COLOR(BRIGHT_RED)"Stream stopped.\n"RESET);

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error_realtime;
    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error_realtime;
    stream = NULL;

error_realtime:
    if (err != paNoError && err != paStreamIsStopped) {
        fprintf(stderr, GET_COLOR(BRIGHT_RED)"PortAudio Realtime Error: %s\n"RESET, Pa_GetErrorText(err));
    }
    if (stream) {
        Pa_AbortStream(stream);
        Pa_CloseStream(stream);
    }

    // Signal the processing thread to terminate
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
// Helper Function: clear_input_buffer
// ==================
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}