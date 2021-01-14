
#include <gio/gio.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>

char *server = NULL;
gint port = 6666;

static GOptionEntry entries[] = {
    {"server", 's', 0, G_OPTION_ARG_STRING, &server, "OpenOCD server",
     "<HOST>{:<PORT>}"},
    {"port", 'p', 0, G_OPTION_ARG_INT, &port, "OpenOCD TCL Port."},
    {0},
};

static void show_help(GOptionContext *opt_ctx)
{
    char *help_str = g_option_context_get_help(opt_ctx, false, NULL);
    fwrite(help_str, 1, strlen(help_str), stdout);
    g_free(help_str);
}

int main(int argc, char *argv[])
{
    GOptionContext *oc;
    GError *error = NULL;
    gssize count;

    setlocale(LC_CTYPE, "");

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
    char *cmd = g_string_free(tmp, false);

    /* Open a TCP connection to the specified port: */
    GSocketClient *client = g_socket_client_new();
    GSocketConnection *con = g_socket_client_connect_to_host(
        client, server ?: "localhost", port, NULL, &error);
    if (con == NULL) {
        g_printerr("connect failed: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    GOutputStream *ostream = g_io_stream_get_output_stream(G_IO_STREAM(con));
    count = g_output_stream_write(ostream, cmd, strlen(cmd), NULL, &error);
    if (count < 0) {
        g_printerr("write failed: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    GInputStream *istream = g_io_stream_get_input_stream(G_IO_STREAM(con));
    char response[1024];

    count =
        g_input_stream_read(istream, response, sizeof(response), NULL, &error);
    if (count < 0) {
        g_printerr("read failed: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    response[count - 1] = '\0';
    printf("%s\n", response);

cleanup:
    g_free(server);
    g_free(cmd);
    g_object_unref(client);
    g_object_unref(con);

    return 0;
}
