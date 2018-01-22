// gcc -o pulseaudio pulseaudio.c -lpulse

#include <pulse/pulseaudio.h>

static void write_callback (pa_stream *stream, size_t nbytes, void *userdata) {
	void *_data = NULL;
	pa_stream_begin_write (stream, &_data, &nbytes);
	float *data = _data;
	for (size_t t = 0; t < nbytes/sizeof(float); ++t) {
		// a simple saw wave
		static float y = -1.f;
		data[t] = y * .2f;
		y += 220.f / 44100.f * 2.f;
		if (y > 1.f) y -= 2.f;
	}
	pa_stream_write (stream, data, nbytes, NULL, 0, PA_SEEK_RELATIVE);
}

static void state_callback (pa_context *context, void *userdata) {
	if (pa_context_get_state(context) == PA_CONTEXT_READY) {
		pa_sample_spec sample_spec = {
			PA_SAMPLE_FLOAT32LE,
			44100,
			1
		};
		pa_stream *stream = pa_stream_new (context, "tutorial", &sample_spec, NULL);
		pa_stream_set_write_callback (stream, write_callback, NULL);
		pa_stream_connect_playback (stream, NULL, NULL, PA_STREAM_NOFLAGS, NULL, NULL);
	}
}

int main () {
	pa_mainloop *mainloop = pa_mainloop_new ();
	pa_context *context = pa_context_new (pa_mainloop_get_api(mainloop), "tutorial");
	pa_context_set_state_callback (context, state_callback, NULL);
	pa_context_connect (context, NULL, PA_CONTEXT_NOFLAGS, NULL);
	pa_mainloop_run (mainloop, NULL);
	pa_context_unref (context);
	pa_mainloop_free (mainloop);
	return 0;
}
