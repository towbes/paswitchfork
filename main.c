#include <stdio.h>

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

pa_mainloop* mainloop;
pa_mainloop_api* mainloop_api;
pa_context* context;

static int setup_context()
{
	mainloop = pa_mainloop_new();
	if (!mainloop) {
        	printf("pa_mainloop_new() failed.\n");
		return -1;
	}

	mainloop_api = pa_mainloop_get_api(mainloop);

        pa_proplist     *proplist;

        proplist = pa_proplist_new();
        pa_proplist_sets(proplist,
                          PA_PROP_APPLICATION_NAME,
                          "name");
        pa_proplist_sets(proplist,
                          PA_PROP_APPLICATION_ID,
                          "id");
        pa_proplist_sets(proplist,
                       PA_PROP_APPLICATION_ICON_NAME,
                       "x");
        pa_proplist_sets(proplist,
                       PA_PROP_APPLICATION_VERSION,
                       "1.0");

	context = pa_context_new_with_proplist(
			mainloop_api, NULL, proplist);
    	if (!context) {
        	printf("pa_context_new() failed.\n");
		return -1;
    	}

        pa_proplist_free(proplist);

    	if(pa_context_connect(context, NULL, 0, NULL) < 0) {
        	printf("pa_context_connect() failed: %s\n", 
				pa_strerror(pa_context_errno(context)));
		return -1;
	}

	return 0;
}

static void context_drain_complete(pa_context *c, void *userdata) {
    pa_context_disconnect(c);
}

static void drain(void) {
    pa_operation *o;

    if (!(o = pa_context_drain(context, context_drain_complete, NULL)))
        pa_context_disconnect(context);
    else
        pa_operation_unref(o);
}


static void stream_restore_cb(pa_context *c,
		const pa_ext_stream_restore_info *info, int eol, void *userdata)
{
	char* name = (char*) userdata;

        pa_ext_stream_restore_info new_info;

        if(eol) {
		drain();
 		//mainloop_api->quit(mainloop_api, 0);
		return;
	}

        new_info.name = info->name;
        new_info.channel_map = info->channel_map;
        new_info.volume = info->volume;
        new_info.mute = info->mute;

        new_info.device = name;

        pa_operation *o;
        o = pa_ext_stream_restore_write (context,
                                         PA_UPDATE_REPLACE,
                                         &new_info, 1,
                                         1, NULL, NULL);
        if(o == NULL) {
                printf("pa_ext_stream_restore_write() failed: %s\n",
                           pa_strerror(pa_context_errno(context)));
                return;
        }

        //printf("Changed default device for %s to %s\n", info->name, info->device);

        pa_operation_unref (o);
}

static int set_default_sink(char* name)
{
        pa_operation *o;

        o = pa_context_set_default_sink(context, name, NULL, NULL);
        if(o == NULL) {
		printf("pa_context_set_default_sink() failed: %s\n",
                           pa_strerror(pa_context_errno(context)));
		return -1;
        }

        pa_operation_unref(o);

        o = pa_ext_stream_restore_read(context,
                                        stream_restore_cb,
					name);

        if(o == NULL) {
                printf("pa_ext_stream_restore_read() failed: %s\n",
                           pa_strerror(pa_context_errno(context)));
                return -1;
        }

        pa_operation_unref(o);

        return 0;
}

static void context_state_callback(pa_context *c, void *userdata)
{
	char* name = (char*) userdata;

	switch(pa_context_get_state(c)) {
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;

		case PA_CONTEXT_READY:
			printf("setting default sink to %s\n", name);

			if(set_default_sink(name)) {
				printf("set_default_sink() failed\n");
    				mainloop_api->quit(mainloop_api, 1);
			}

			break;

		case PA_CONTEXT_TERMINATED:
    			mainloop_api->quit(mainloop_api, 0);

		default:
			printf("connection failure: %s\n",
					pa_strerror(pa_context_errno(c)));
	}
}

int main(int argc, char** argv) {
	if(setup_context()) {
		printf("can't get pulseaudio context.\n");
		return 1;
	}

	char* name = argv[1];
	pa_context_set_state_callback(context, context_state_callback, name);

	int ret;
	if(pa_mainloop_run(mainloop, &ret) < 0) {
		printf("pa_mainloop_run() failed.\n");
		return 1;
	}

	pa_context_disconnect(context);
        pa_context_unref(context);
        pa_signal_done();
        pa_mainloop_free(mainloop);

	return ret;
}
