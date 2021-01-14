
#include <gio/gio.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>

char *server = NULL;
int port = 6666;

static GOptionEntry entries[] = {
    {"server", 's', 0, G_OPTION_ARG_STRING, &server, "OpenOCD server",
     "<HOST>{:<PORT>}"},
    {"port", 'p', 0, G_OPTION_ARG_INT, &port, "OpenOCD Server port", "<PORT>"},
    {0},
};

typedef struct {
    char *cmd;
    GMainLoop *loop;
    char response[1024];
} Context;

static void context_clean(Context *ctx)
{
    g_free(ctx->cmd);
    g_main_loop_unref(ctx->loop);
}

static void show_help(GOptionContext *opt_ctx)
{
    char *help_str = g_option_context_get_help(opt_ctx, false, NULL);
    fwrite(help_str, 1, strlen(help_str), stdout);
    g_free(help_str);
}

static void process_response(GObject *source, GAsyncResult *result,
                             gpointer user_data)
{
    Context *ctx = (Context *)user_data;
    gssize count = 0;
    GError *error = NULL;

    count = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);
    if (count < 0) {
        g_printerr("%s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(ctx->loop);
        return;
    }

    ctx->response[count - 1] = '\0';
    printf("%s\n", ctx->response);
    g_main_loop_quit(ctx->loop);

    return;
}

/* Callback: Command Finished. */
static void send_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Context *ctx = (Context *)user_data;
    gsize written = 0;
    GError *error = NULL;

    if (!g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result,
                                          &written, &error)) {
        g_printerr("%s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(ctx->loop);
        return;
    }
    return;
}

/* Callback: Connected to OpenOCD server. */
static void connected(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Context *ctx = (Context *)user_data;
    GError *error = NULL;
    GSocketConnection *con;
    GInputStream *istream;
    GOutputStream *ostream;

    con = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
                                                 result, &error);
    if (con == NULL) {
        g_printerr("connect failed: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(ctx->loop);
        return;
    }

    istream = g_io_stream_get_input_stream(G_IO_STREAM(con));
    ostream = g_io_stream_get_output_stream(G_IO_STREAM(con));

    /* Process the OpenOCD response (async) */
    g_input_stream_read_async(istream, ctx->response, sizeof(ctx->response),
                              G_PRIORITY_DEFAULT, NULL, process_response,
                              user_data);

    /* Issue the OpenOCD command (async) */
    g_output_stream_write_all_async(ostream, ctx->cmd, strlen(ctx->cmd),
                                    G_PRIORITY_DEFAULT, NULL, send_done,
                                    user_data);

    g_object_unref(con);
}

int main(int argc, char *argv[])
{
    GOptionContext *oc;
    GError *error = NULL;
    GSocketClient *client;
    Context ctx = {0};

    setlocale(LC_ALL, "");

    /* Process CLI arguments: */
    oc = g_option_context_new("<openocd_cmd> [<arg> ...]");
    g_option_context_add_main_entries(oc, entries, NULL);
    g_option_context_set_strict_posix(oc, true);
    if (!g_option_context_parse(oc, &argc, &argv, &error)) {
        g_printerr("%s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (argc < 2) {
        show_help(oc);
        exit(1);
    }
    g_option_context_free(oc);

    /* Construct OpenOCD command:
     *
     * NOTE: Command string must be terminated with 0x1A
     */
    GString *tmp = g_string_new(argv[1]);
    for (int i = 2; i < argc; i++)
        g_string_append_printf(tmp, " %s", argv[i]);
    g_string_append_c(tmp, 0x1a);
    ctx.cmd = g_string_free(tmp, false);

    ctx.loop = g_main_loop_new(NULL, false);

    /* Connect to OpenOCD server (async) */
    client = g_socket_client_new();
    g_socket_client_connect_to_host_async(client, server ?: "localhost", port,
                                          NULL, connected, &ctx);

    /* Let's do this! */
    g_main_loop_run(ctx.loop);

    /* Cleanup */
    g_object_unref(client);
    context_clean(&ctx);
    g_free(server);

    return 0;
}
